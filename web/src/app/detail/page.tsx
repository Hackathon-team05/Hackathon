import fs from "fs";
import path from "path";

export default function DetailPage() {
  const html = fs.readFileSync(
    path.join(process.cwd(), "src/data/detail.html"),
    "utf8"
  );
  return <div dangerouslySetInnerHTML={{ __html: html }} />;
}
