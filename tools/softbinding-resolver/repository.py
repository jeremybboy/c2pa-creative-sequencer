import hashlib
import json
import os
import pathlib
import tempfile
import threading

from constants import ALGORITHM


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
        self.index_path = self.root / "index.json"
        self.lock = threading.RLock()
        self.root.mkdir(parents=True, exist_ok=True)
        self.manifests.mkdir(parents=True, exist_ok=True)
        if not self.index_path.exists():
            self._write({"version": 1, "manifests": {}, "bindings": {}, "imports": {}})

    def _read(self):
        try:
            data = json.loads(self.index_path.read_text())
        except (OSError, json.JSONDecodeError) as error:
            raise ValueError(f"invalid resolver repository index: {error}") from error
        for key in ("manifests", "bindings", "imports"):
            if not isinstance(data.get(key), dict):
                raise ValueError(f"resolver repository index has no {key} map")
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
                fingerprint = hashlib.sha256(
                    (package / "binding.json").read_bytes() + manifest
                ).hexdigest()
                with self.lock:
                    index = self._read()
                    if index["imports"].get(str(package)) == fingerprint:
                        result["alreadyImported"] += 1
                        continue
                algorithm = binding.get("algorithm", "")
                value = binding.get("value", "").lower()
                manifest_id = binding.get("manifestId", "")
                self.store_manifest(manifest, manifest_id)
                self.add_binding(algorithm, value, manifest_id, binding)
                with self.lock:
                    index = self._read()
                    index["imports"][str(package)] = fingerprint
                    self._write(index)
                result["imported"] += 1
            except (OSError, ValueError, json.JSONDecodeError) as error:
                result["errors"].append({"package": str(package), "error": str(error)})
        return result
