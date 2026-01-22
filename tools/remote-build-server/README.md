# Remote Build/Test Server (iOS)

Minimal HTTP service for iOS remote build/test queueing.

## Endpoints

- `GET /health` → `{ "status": "ok" }`
- `POST /api/build` → queues a build job
- `POST /api/test` → queues a test job
- `GET /api/jobs` → returns job list (latest first)

## Payload

```
{
  "kind": "build",
  "payload": {
    "project_path": "/path/to/project"
  }
}
```

## Run

```
python3 server.py
```

Defaults to `http://0.0.0.0:8787`.
