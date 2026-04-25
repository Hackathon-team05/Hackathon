import type { Metadata } from "next";
import "./globals.css";
import Nav from "@/components/Nav";
import MermaidInitializer from "@/components/MermaidInitializer";

export const metadata: Metadata = {
  title: "Arduinoオーケストラ 設計書",
  description: "基本設計・詳細設計・担当割り振り・ガントチャート",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="ja">
      <body>
        <MermaidInitializer />
        <Nav />
        <main className="max-w-[1400px] mx-auto px-12 py-8 max-md:px-4 max-md:py-5">
          {children}
        </main>
      </body>
    </html>
  );
}
