import hashlib
import json
import os
import pathlib
import tempfile
import threading

from constants import ALGORITHM, FINGERPRINT_ALGORITHM


def default_repository_root():
    return pathlib.Path.home() / "Library/Application Support/C2PA Soft Binding Demo/repository"


def default_outbox_root():
    return pathlib.Path.home() / "Library/Application Support/C2PA Creative Sequencer/SoftBindingOutbox"


def valid_binding(value):
    return len(value) == 32 and all(character in "0123456789abcdef" for character in value)


class Repository:
    def __init__(self, root=None):
        self.root = pathlib.Path(root or default_repository_root())
        self.manifests = self.root / "manifests"
        self.fingerprints = self.root / "fingerprints"
        self.index_path = self.root / "index.json"
        self.lock = threading.RLock()
        self.root.mkdir(parents=True, exist_ok=True)
        self.manifests.mkdir(parents=True, exist_ok=True)
        self.fingerprints.mkdir(parents=True, exist_ok=True)
        if not self.index_path.exists():
            self._write({"version": 2, "manifests": {}, "bindings": {},
                         "fingerprints": {}, "imports": {}})
        else:
            data = self._read()
            if "fingerprints" not in data:
                data["fingerprints"] = {}
                data["version"] = 2
                self._write(data)

    def _read(self):
        try:
            data = json.loads(self.index_path.read_text())
        except (OSError, json.JSONDecodeError) as error:
            raise ValueError(f"invalid resolver repository index: {error}") from error
        for key in ("manifests", "bindings", "imports"):
            if not isinstance(data.get(key), dict):
                raise ValueError(f"resolver repository index has no {key} map")
        data.setdefault("fingerprints", {})
        return data

    def _write(self, data):
        self.root.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(prefix="index-", suffix=".json", dir=self.root)
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
                json.dump(data, stream, indent=2, sort_keys=True)
                stream.write("\n")
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, self.index_path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    def store_manifest(self, manifest_bytes, manifest_id):
        if not manifest_bytes or not manifest_id:
            raise ValueError("manifest bytes and active manifest identifier are required")
        digest = hashlib.sha256(manifest_bytes).hexdigest()
        path = self.manifests / f"{digest}.c2pa"
        with self.lock:
            if path.exists() and path.read_bytes() != manifest_bytes:
                raise ValueError("manifest hash collision")
            if not path.exists():
                descriptor, temporary = tempfile.mkstemp(
                    prefix="manifest-", suffix=".c2pa", dir=self.manifests
                )
                try:
                    with os.fdopen(descriptor, "wb") as stream:
                        stream.write(manifest_bytes)
                        stream.flush()
                        os.fsync(stream.fileno())
                    os.replace(temporary, path)
                finally:
                    if os.path.exists(temporary):
                        os.unlink(temporary)
            index = self._read()
            index["manifests"][manifest_id] = {
                "path": f"manifests/{path.name}", "sha256": digest
            }
            self._write(index)
        return manifest_id

    def add_binding(self, algorithm, value, manifest_id, metadata=None):
        value = value.lower()
        if algorithm != ALGORITHM or not valid_binding(value):
            raise ValueError("unsupported algorithm or invalid 128-bit binding value")
        with self.lock:
            index = self._read()
            if manifest_id not in index["manifests"]:
                raise ValueError("binding references an unknown manifest")
            key = f"{algorithm}:{value}"
            entry = {"manifestId": manifest_id, "metadata": metadata or {}}
            existing = index["bindings"].setdefault(key, [])
            if not any(item.get("manifestId") == manifest_id for item in existing):
                existing.append(entry)
            self._write(index)

    def matches(self, algorithm, value):
        key = f"{algorithm}:{value.lower()}"
        with self.lock:
            return list(self._read()["bindings"].get(key, []))

    def add_fingerprint(self, algorithm, value, manifest_id, artifact, metadata=None):
        value = value.lower()
        artifact = pathlib.Path(artifact)
        if algorithm != FINGERPRINT_ALGORITHM or len(value) != 64:
            raise ValueError("unsupported fingerprint algorithm or value")
        data = artifact.read_bytes()
        if hashlib.sha256(data).hexdigest() != value or not data.startswith(b"audfprinthashV00"):
            raise ValueError("fingerprint artifact does not match its registration value")
        registration_id = hashlib.sha256(f"{manifest_id}:{value}".encode()).hexdigest()
        destination = self.fingerprints / f"{registration_id}.afpt"
        with self.lock:
            index = self._read()
            if manifest_id not in index["manifests"]:
                raise ValueError("fingerprint references an unknown manifest")
            if not destination.exists():
                destination.write_bytes(data)
            key = f"{algorithm}:{value}"
            entries = index["fingerprints"].setdefault(key, [])
            entry = {
                "registrationId": registration_id,
                "manifestId": manifest_id,
                "value": value,
                "algorithm": algorithm,
                "path": f"fingerprints/{destination.name}",
                "metadata": metadata or {},
            }
            if not any(item.get("registrationId") == registration_id for item in entries):
                entries.append(entry)
            self._write(index)

    def fingerprint_registrations(self):
        with self.lock:
            return [item for entries in self._read()["fingerprints"].values()
                    for item in entries]

    def fingerprint_registration(self, registration_id):
        for item in self.fingerprint_registrations():
            if item.get("registrationId") == registration_id:
                return item
        return None

    def get_manifest(self, manifest_id):
        with self.lock:
            entry = self._read()["manifests"].get(manifest_id)
            if not entry:
                return None
            path = self.root / entry["path"]
            data = path.read_bytes()
            if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                raise ValueError("stored manifest hash mismatch")
            return data

    def import_outbox(self, outbox=None):
        outbox = pathlib.Path(outbox or default_outbox_root())
        if not outbox.exists():
            return {"imported": 0, "alreadyImported": 0, "errors": []}
        result = {"imported": 0, "alreadyImported": 0, "errors": []}
        for package in sorted(path for path in outbox.iterdir() if path.is_dir()):
            try:
                binding = json.loads((package / "binding.json").read_text())
                manifest = (package / binding.get("manifestFile", "manifest.c2pa")).read_bytes()
                package_bytes = (package / "binding.json").read_bytes() + manifest
                for optional in ("fingerprint.json", "fingerprint-data.afpt"):
                    if (package / optional).exists():
                        package_bytes += (package / optional).read_bytes()
                fingerprint = hashlib.sha256(package_bytes).hexdigest()
                with self.lock:
                    index = self._read()
                    if index["imports"].get(str(package)) == fingerprint:
                        result["alreadyImported"] += 1
                        continue
                manifest_id = binding.get("manifestId", "")
                self.store_manifest(manifest, manifest_id)
                bindings = binding.get("bindings")
                if not isinstance(bindings, list):
                    bindings = [{"type": "watermark", "algorithm": binding.get("algorithm", ""),
                                 "value": binding.get("value", "")}]
                for item in bindings:
                    algorithm = item.get("algorithm", "")
                    value = item.get("value", "").lower()
                    if item.get("type") == "fingerprint":
                        registration_file = item.get("registrationFile", "fingerprint-data.afpt")
                        self.add_fingerprint(algorithm, value, manifest_id,
                                             package / registration_file, binding)
                    elif item.get("type") == "watermark":
                        self.add_binding(algorithm, value, manifest_id, binding)
                with self.lock:
                    index = self._read()
                    index["imports"][str(package)] = fingerprint
                    self._write(index)
                result["imported"] += 1
            except (OSError, ValueError, json.JSONDecodeError) as error:
                result["errors"].append({"package": str(package), "error": str(error)})
        return result
