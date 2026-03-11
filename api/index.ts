import { spawn, ChildProcess } from 'child_process';
import { existsSync } from 'fs';
import path from 'path';

// Basic Node.js API server placeholder
console.log('Advanced Database API starting...');

const repoRoot: string = existsSync(path.resolve(process.cwd(), 'build'))
  ? process.cwd()
  : path.resolve(process.cwd(), '..');

const preferredBinaryName: string = process.env.DBCORE_BINARY ?? 'dbcore_engine';
const fallbackBinaryName: string = 'dbcore_test';

const candidatePaths: string[] = [
  path.resolve(repoRoot, 'build', 'engine', `${preferredBinaryName}.exe`),
  path.resolve(repoRoot, 'build', 'engine', preferredBinaryName),
  path.resolve(repoRoot, 'build', 'engine', `${fallbackBinaryName}.exe`),
  path.resolve(repoRoot, 'build', 'engine', fallbackBinaryName)
];

const dbExecutablePath: string | undefined = candidatePaths.find((candidate) => existsSync(candidate));
const dbArgs: string[] = (process.env.DBCORE_ARGS ?? '').trim() === ''
  ? []
  : (process.env.DBCORE_ARGS as string).split(' ');

if (!dbExecutablePath) {
  console.warn('No C++ database executable found. Build C++ targets first (cmake --build build).');
} else {
  console.log(`Launching C++ engine binary: ${dbExecutablePath}`);
  const dbProcess: ChildProcess = spawn(dbExecutablePath, dbArgs, { stdio: 'inherit' });

  dbProcess.on('error', (err: NodeJS.ErrnoException) => {
    console.error(`Failed to start database process: ${err.message}`);
  });

  dbProcess.on('close', (code: number | null) => {
    console.log(`Database process exited with code ${code}`);
  });
}