import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Moria Online — The Shared Deep",
  description: "Explore a shared dungeon, battle monsters, and conquer the deep with other adventurers.",
  other: {
    "codex-preview": "development",
  },
  icons: {
    icon: "/favicon.svg",
    shortcut: "/favicon.svg",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className="antialiased">{children}</body>
    </html>
  );
}
