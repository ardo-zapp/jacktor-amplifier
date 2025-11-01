import React, { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import { z } from 'zod';

const cacheSchema = z.object({
  etag: z.string().default(''),
  data: z.record(z.unknown()).default({}),
});

type NvCache = z.infer<typeof cacheSchema>;

type NvContextValue = {
  cache: NvCache;
  refresh: () => Promise<void>;
  setValue: (key: string, value: unknown) => Promise<void>;
};

const NvContext = createContext<NvContextValue | undefined>(undefined);

async function loadCache(): Promise<NvCache> {
  try {
    const raw = await window.panelSerial?.readCache();
    if (raw) {
      return cacheSchema.parse(raw);
    }
  } catch (err) {
    console.warn('Failed to read NV cache', err);
  }
  return cacheSchema.parse({});
}

async function persistCache(cache: NvCache) {
  try {
    await window.panelSerial?.writeCache(cache);
  } catch (err) {
    console.warn('Failed to persist NV cache', err);
  }
}

export const NvSyncProvider: React.FC<React.PropsWithChildren> = ({ children }) => {
  const [cache, setCache] = useState<NvCache>({ etag: '', data: {} });

  useEffect(() => {
    loadCache().then(setCache);
  }, []);

  const send = useCallback(async (line: string) => {
    await window.panelSerial?.sendCli(line);
  }, []);

  const refresh = useCallback(async () => {
    await send('nv_digest?');
  }, [send]);

  const setValue = useCallback(
    async (key: string, value: unknown) => {
      const nextEtag = cache.etag || '0';
      const payload = JSON.stringify({ key, value, etag: nextEtag });
      await send(`nv_set ${payload}`);
    },
    [cache.etag, send]
  );

  useEffect(() => {
    const handler = (frame: unknown) => {
      if (!frame || typeof frame !== 'object') {
        return;
      }
      const typed = frame as Record<string, unknown>;
      if (typed.type === 'nv_digest' && typeof typed.etag === 'string') {
        const next = { etag: typed.etag, data: cache.data };
        setCache(next);
        persistCache(next);
        if (typed.stale) {
          send('nv_get');
        }
      }
      if (typed.type === 'nv_get' && typeof typed.payload === 'object') {
        const next = { etag: String(typed.etag ?? cache.etag), data: typed.payload as Record<string, unknown> };
        setCache(next);
        persistCache(next);
      }
    };
    const dispose = window.panelSerial?.onTelemetry(handler as any);
    return () => {
      dispose && dispose();
    };
  }, [cache.data, cache.etag, send]);

  const value = useMemo<NvContextValue>(
    () => ({
      cache,
      refresh,
      setValue,
    }),
    [cache, refresh, setValue]
  );

  return <NvContext.Provider value={value}>{children}</NvContext.Provider>;
};

export const useNvSync = () => {
  const ctx = useContext(NvContext);
  if (!ctx) {
    throw new Error('useNvSync must be used within NvSyncProvider');
  }
  return ctx;
};
