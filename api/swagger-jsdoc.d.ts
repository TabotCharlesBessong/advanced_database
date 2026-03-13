// Type declarations for swagger-jsdoc
// This is a workaround for missing @types/swagger-jsdoc package

declare module 'swagger-jsdoc' {
  export interface SwaggerDefinition {
    definition?: any;
    apis?: string[];
  }

  function swaggerJsdoc(options: SwaggerDefinition): any;

  export default swaggerJsdoc;
}
