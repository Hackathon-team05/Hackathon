"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";

const navItems = [
  { href: "/",         label: "概要" },
  { href: "/basic",    label: "基本設計" },
  { href: "/detail",   label: "詳細設計" },
  { href: "/assign",   label: "担当割り振り" },
  { href: "/schedule", label: "スケジュール" },
  { href: "/nonfunc",  label: "非機能要件" },
  { href: "/issues",   label: "未確定事項" },
];

export default function Nav() {
  const pathname = usePathname();

  return (
    <nav className="bg-[#1a252f] sticky top-0 z-50 flex flex-wrap gap-1 px-5 py-2">
      <span className="text-[#ecf0f1] font-bold text-sm px-3.5 py-1.5 mr-2 border-r border-[#34495e]">
        🎵 Arduinoオーケストラ 設計書
      </span>
      {navItems.map(({ href, label }) => {
        const isActive = pathname === href;
        return (
          <Link
            key={href}
            href={href}
            className={`text-[#bdc3c7] no-underline px-3.5 py-1.5 rounded text-[13px] transition-colors duration-200 hover:bg-[#2980b9] hover:text-white ${
              isActive ? "bg-[#2980b9] text-white" : ""
            }`}
          >
            {label}
          </Link>
        );
      })}
    </nav>
  );
}
