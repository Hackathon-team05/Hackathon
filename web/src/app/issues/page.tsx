import fs from "fs";
import path from "path";

export default function IssuesPage() {
  const html = fs.readFileSync(
    path.join(process.cwd(), "src/data/issues.html"),
    "utf8"
  );
  return <div dangerouslySetInnerHTML={{ __html: html }} />;
}
