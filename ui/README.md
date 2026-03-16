# Advanced Database UI (Phase 6 Week 33-34)

This React app now covers the first three Phase 6 UI milestones:

- React development environment (Vite + TypeScript)
- Basic layout and navigation
- Connection management
- SQL editor component with API execution
- Table explorer and schema browser
- Query history and favorites
- Import/export tools
- User preferences and performance dashboard

## Features

- **Connection panel**: API URL, username, password, connect/disconnect
- **Session initialization**: runs `/auth/login` then `/init`
- **SQL workspace**: sends statements to `/sql`
- **Result viewer**: grid + raw JSON output
- **Table browser**: load schemas and rows from `/tables` endpoints
- **Query library**: local history and reusable favorites
- **Import/export**: load `.sql` files, export JSON/CSV results
- **Preferences**: local editor and browser settings
- **Performance dashboard**: request timing summaries
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
- `src/components/QueryResult.tsx`: grid/raw results display
- `src/components/TableExplorer.tsx`: table browser
- `src/components/SchemaDiagram.tsx`: schema visualization
- `src/components/QueryLibrary.tsx`: query history + favorites
- `src/components/ImportExportPanel.tsx`: file import and export actions
- `src/components/PreferencesPanel.tsx`: local settings controls
- `src/components/PerformanceDashboard.tsx`: request metrics view
- `src/api/client.ts`: typed API client helpers
