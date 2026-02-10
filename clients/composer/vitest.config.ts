import { fileURLToPath } from 'node:url'
import { mergeConfig, defineConfig, configDefaults } from 'vitest/config'
import viteConfig from './vite.config'

export default defineConfig(({ mode }) => {
  const baseConfig =
    typeof viteConfig === 'function' ? viteConfig({ command: 'serve', mode, isSsrBuild: false, isPreview: false }) : viteConfig

  return mergeConfig(
    baseConfig,
    defineConfig({
      test: {
        environment: 'jsdom',
        exclude: [...configDefaults.exclude, 'e2e/**', 'node_modules.bak/**'],
        root: fileURLToPath(new URL('./', import.meta.url)),
        setupFiles: ['./src/__tests__/setup.ts'],
      },
    }),
  )
})
