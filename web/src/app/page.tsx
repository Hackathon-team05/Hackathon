import fs from "fs";
import path from "path";

export default function CoverPage() {
  const html = fs.readFileSync(
    path.join(process.cwd(), "src/data/cover.html"),
    "utf8"
  );
  return <div dangerouslySetInnerHTML={{ __html: html }} />;
}
