import { FormEvent, useEffect, useMemo, useState } from 'react';
import { TableSchema } from '../api/client';

interface RowEditorDialogProps {
  open: boolean;
  schema: TableSchema | null;
  busy: boolean;
  onClose: () => void;
  onSubmit: (values: Record<string, string | number | null>) => Promise<void>;
}

function toPayload(
  schema: TableSchema,
  values: Record<string, string>
): { ok: true; payload: Record<string, string | number | null> } | { ok: false; error: string } {
  const payload: Record<string, string | number | null> = {};

  for (const column of schema.columns) {
    const raw = (values[column.name] ?? '').trim();

    if (raw.length === 0) {
      if (column.nullable) {
        payload[column.name] = null;
        continue;
      }
      return { ok: false, error: `Column ${column.name} is required.` };
    }

    if (column.type.toLowerCase().startsWith('int')) {
      const parsed = Number(raw);
      if (!Number.isInteger(parsed)) {
        return { ok: false, error: `Column ${column.name} expects an integer.` };
      }
      payload[column.name] = parsed;
      continue;
    }

    payload[column.name] = raw;
  }

  return { ok: true, payload };
}

export function RowEditorDialog({
  open,
  schema,
  busy,
  onClose,
  onSubmit,
}: RowEditorDialogProps) {
  const [values, setValues] = useState<Record<string, string>>({});
  const [error, setError] = useState<string | null>(null);

  const fields = useMemo(() => schema?.columns ?? [], [schema]);

  useEffect(() => {
    if (!open || !schema) {
      return;
    }

    const initialValues: Record<string, string> = {};
    for (const column of schema.columns) {
      initialValues[column.name] = '';
    }

    setValues(initialValues);
    setError(null);
  }, [open, schema]);

  if (!open || !schema) {
    return null;
  }

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();

    const conversion = toPayload(schema, values);
    if (!conversion.ok) {
      setError(conversion.error);
      return;
    }

    setError(null);
    await onSubmit(conversion.payload);
  };

  return (
    <div className="dialog-backdrop" role="presentation">
      <section className="dialog" role="dialog" aria-modal="true" aria-label="Insert row">
        <header className="panel-header compact">
          <h2>Insert Row · {schema.name}</h2>
          <button type="button" className="secondary" onClick={onClose} disabled={busy}>
            Close
          </button>
        </header>

        <form className="form" onSubmit={handleSubmit}>
          {fields.map((column) => (
            <label key={column.name}>
              {column.name} ({column.type})
              <input
                value={values[column.name] ?? ''}
                onChange={(event) => {
                  const next = event.target.value;
                  setValues((previous) => ({ ...previous, [column.name]: next }));
                }}
                disabled={busy}
                placeholder={column.nullable ? 'Optional' : 'Required'}
              />
            </label>
          ))}

          {error && <p className="error-text">{error}</p>}

          <div className="button-row">
            <button type="submit" disabled={busy}>
              {busy ? 'Saving...' : 'Insert Row'}
            </button>
          </div>
        </form>
      </section>
    </div>
  );
}
