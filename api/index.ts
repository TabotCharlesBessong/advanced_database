import { spawn, ChildProcess } from 'child_process';
import { existsSync } from 'fs';
import path from 'path';

// Basic Node.js API server placeholder
console.log('Advanced Database API starting...');

// Resolve dbcore_test path from likely locations whether API runs from repo root or api/
const candidatePaths: string[] = [
  path.resolve(process.cwd(), 'build', 'engine', 'dbcore_test.exe'),
  path.resolve(process.cwd(), 'build', 'engine', 'dbcore_test'),
  path.resolve(process.cwd(), '..', 'build', 'engine', 'dbcore_test.exe'),
  path.resolve(process.cwd(), '..', 'build', 'engine', 'dbcore_test')
];

const dbExecutablePath: string | undefined = candidatePaths.find((candidate) => existsSync(candidate));

if (!dbExecutablePath) {
  console.warn('dbcore_test executable not found. Build C++ engine first (cmake --build build).');
} else {
  const dbProcess: ChildProcess = spawn(dbExecutablePath, [], { stdio: 'inherit' });

  dbProcess.on('error', (err: NodeJS.ErrnoException) => {
    console.error(`Failed to start database process: ${err.message}`);
  });

  dbProcess.on('close', (code: number | null) => {
    console.log(`Database process exited with code ${code}`);
  });
}