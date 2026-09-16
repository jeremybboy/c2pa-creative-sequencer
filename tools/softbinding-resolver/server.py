#!/usr/bin/env python3
import argparse
import json
import mimetypes
import pathlib
import sys
import threading
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from audiowmark_decoder import AudioWMarkDecoder
from constants import ALGORITHM
from repository import Repository, default_outbox_root
from service import ResolverService


def handler_factory(repository, service, outbox, static_root=None):
    static_root = pathlib.Path(static_root or ROOT / "static")

    class Handler(BaseHTTPRequestHandler):
        server_version = "C2PASoftBindingDemo/0.1"

        def log_message(self, format_string, *args):
            print(f"[{self.log_date_time_string()}] {format_string % args}")

        def send_json(self, status, value):
            data = json.dumps(value).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def read_body(self):
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0:
                raise ValueError("request body is required")
            return self.rfile.read(length)

        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path == "/services/supportedAlgorithms":
                return self.send_json(200, {"algorithms": [ALGORITHM], "conformant": False})
            if parsed.path == "/matches/byBinding":
                query = urllib.parse.parse_qs(parsed.query)
                algorithm = query.get("algorithm", [""])[0]
                value = query.get("value", [""])[0]
                return self.send_json(200, {
                    "matches": repository.matches(algorithm, value),
                    "algorithm": algorithm,
                    "value": value,
                })
            if parsed.path.startswith("/manifests/"):
                manifest_id = urllib.parse.unquote(parsed.path[len("/manifests/"):])
                data = repository.get_manifest(manifest_id)
                if data is None:
                    return self.send_json(404, {"error": "manifest not found"})
                self.send_response(200)
                self.send_header("Content-Type", "application/c2pa")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            relative = "index.html" if parsed.path == "/" else parsed.path.lstrip("/")
            file = (static_root / relative).resolve()
            if static_root.resolve() not in file.parents or not file.is_file():
                return self.send_json(404, {"error": "not found"})
            data = file.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", mimetypes.guess_type(file.name)[0] or "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_POST(self):
            parsed = urllib.parse.urlparse(self.path)
            try:
                if parsed.path == "/manifests":
                    manifest_id = self.headers.get("X-C2PA-Manifest-Id", "")
                    stored = repository.store_manifest(self.read_body(), manifest_id)
                    return self.send_json(201, {"manifestId": stored})
                if parsed.path == "/bindings":
                    request = json.loads(self.read_body())
                    repository.add_binding(
                        request.get("algorithm", ""), request.get("value", ""),
                        request.get("manifestId", ""), request.get("metadata", {})
                    )
                    return self.send_json(201, {"stored": True})
                if parsed.path == "/matches/byContent":
                    filename = self.headers.get("X-Filename", "audio.wav")
                    suffix = pathlib.Path(filename).suffix or ".wav"
                    return self.send_json(200, service.match_content_bytes(self.read_body(), suffix))
                if parsed.path == "/imports/sequencer":
                    return self.send_json(200, repository.import_outbox(outbox))
                return self.send_json(404, {"error": "not found"})
            except (ValueError, OSError, json.JSONDecodeError) as error:
                return self.send_json(400, {"error": str(error)})
            except Exception as error:
                return self.send_json(500, {"error": str(error)})

    return Handler


def make_server(host, port, repository=None, decoder=None, outbox=None, static_root=None):
    repository = repository or Repository()
    decoder = decoder or AudioWMarkDecoder()
    service = ResolverService(repository, decoder)
    return ThreadingHTTPServer(
        (host, port), handler_factory(repository, service, outbox or default_outbox_root(), static_root)
    )


def main():
    parser = argparse.ArgumentParser(description="C2PA SBR-inspired local demonstration service")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    parser.add_argument("--repository")
    parser.add_argument("--outbox")
    parser.add_argument("--audiowmark")
    args = parser.parse_args()
    repository = Repository(args.repository)
    outbox = pathlib.Path(args.outbox) if args.outbox else default_outbox_root()
    imported = repository.import_outbox(outbox)
    server = make_server(
        args.host, args.port, repository,
        AudioWMarkDecoder(args.audiowmark) if args.audiowmark else None, outbox
    )
    print(f"Imported {imported['imported']} Sequencer publication(s); repository ready")
    print(f"Audio Soft-Binding Recovery Demo: http://{args.host}:{server.server_port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
