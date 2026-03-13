# Advanced Database UI (Phase 6 Week 29-30)

This React app implements the **Week 29-30 UI foundation** goals:

- React development environment (Vite + TypeScript)
- Basic layout and navigation
- Connection management
- SQL editor component with API execution

## Features

- **Connection panel**: API URL, username, password, connect/disconnect
- **Session initialization**: runs `/auth/login` then `/init`
- **SQL workspace**: sends statements to `/sql`
- **Result viewer**: pretty JSON output
- **Docs navigation**: quick link to backend Swagger `/api-docs`

## Run

From the `ui` folder:

```bash
npm install
npm run dev
```

UI runs at `http://localhost:5173` by default.

## Backend dependency

The UI expects your API from Phase 5 to be running, defaulting to:

- `http://localhost:3000`

## Structure

- `src/App.tsx`: app shell, view navigation, state orchestration
- `src/components/ConnectionPanel.tsx`: login + connect controls
- `src/components/SqlEditor.tsx`: SQL editor and execute action
- `src/components/QueryResult.tsx`: JSON results display
- `src/api/client.ts`: typed API client helpers
