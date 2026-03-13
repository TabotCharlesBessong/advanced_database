import jwt, { JwtPayload } from 'jsonwebtoken';
import { Request, Response, NextFunction } from 'express';

// JWT configuration
const JWT_SECRET = process.env.JWT_SECRET || 'dev-secret-key-change-in-production';
const JWT_EXPIRY = process.env.JWT_EXPIRY || '24h';

// Extended JWT payload type
export interface AuthPayload extends JwtPayload {
  userId: string;
  username: string;
  iat: number;
  exp: number;
}

// Request type with optional user info
declare global {
  namespace Express {
    interface Request {
      user?: AuthPayload;
    }
  }
}

/**
 * Generate a JWT token for the given user credentials
 * @param userId User identifier
 * @param username Username
 * @returns JWT token string
 */
export function generateToken(userId: string, username: string): string {
  const payload: Omit<AuthPayload, 'iat' | 'exp'> = {
    userId,
    username,
  };
  const signOptions = {
    expiresIn: JWT_EXPIRY as string | number,
  };
  return jwt.sign(payload, JWT_SECRET, signOptions as any);
}

/**
 * Verify and decode a JWT token
 * @param token JWT token string
 * @returns Decoded token payload or throws error
 */
export function verifyToken(token: string): AuthPayload {
  try {
    return jwt.verify(token, JWT_SECRET) as AuthPayload;
  } catch (error) {
    if (error instanceof jwt.TokenExpiredError) {
      throw new Error('Token has expired');
    }
    if (error instanceof jwt.JsonWebTokenError) {
      throw new Error('Invalid token');
    }
    throw error;
  }
}

/**
 * Express middleware to verify JWT from Authorization header
 * Header format: "Bearer <token>"
 */
export function authMiddleware(
  req: Request,
  res: Response,
  next: NextFunction
): void {
  const authHeader = req.headers.authorization;

  if (!authHeader) {
    res.status(401).json({ error: 'Missing authorization header' });
    return;
  }

  const parts = authHeader.split(' ');
  if (parts.length !== 2 || parts[0] !== 'Bearer') {
    res.status(401).json({ error: 'Invalid authorization header format' });
    return;
  }

  const token = parts[1];

  try {
    req.user = verifyToken(token);
    next();
  } catch (error) {
    res.status(401).json({
      error: error instanceof Error ? error.message : 'Authentication failed',
    });
  }
}

/**
 * Optional auth middleware - attaches user if token is valid, but doesn't fail if missing
 */
export function optionalAuthMiddleware(
  req: Request,
  res: Response,
  next: NextFunction
): void {
  const authHeader = req.headers.authorization;

  if (authHeader) {
    const parts = authHeader.split(' ');
    if (parts.length === 2 && parts[0] === 'Bearer') {
      try {
        req.user = verifyToken(parts[1]);
      } catch {
        // Silently ignore invalid tokens in optional mode
      }
    }
  }

  next();
}
