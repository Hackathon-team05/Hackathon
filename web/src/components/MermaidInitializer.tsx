'use client';

import { useEffect } from 'react';
import { usePathname } from 'next/navigation';

export default function MermaidInitializer() {
  const pathname = usePathname();

  useEffect(() => {
    import('mermaid').then((m) => {
      m.default.initialize({ startOnLoad: false, theme: 'default' });
      m.default.run();
    });
  }, [pathname]);

  return null;
}
