import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';

// In dev the /api calls are proxied to the thermostat so the browser sees one origin.
// Set THERMOSTAT_URL (e.g. in web/.env.local) to the device's address.
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, '.', '');
  return {
    plugins: [react()],
    server: {
      proxy: {
        '/api': env.THERMOSTAT_URL || 'http://thermostat.local',
      },
    },
    build: {
      cssCodeSplit: false,
      // The ESP32 serves dist/ from a SPIFFS partition, which has no directories and a
      // 32 character limit on file names, so emit short, flat, hashed names.
      rollupOptions: {
        output: {
          entryFileNames: '[hash:8].js',
          chunkFileNames: '[hash:8].js',
          assetFileNames: '[hash:8][extname]',
        },
      },
    },
  };
});
