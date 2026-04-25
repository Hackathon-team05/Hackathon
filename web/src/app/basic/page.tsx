import fs from "fs";
import path from "path";

export default function BasicPage() {
  const html = fs.readFileSync(
    path.join(process.cwd(), "src/data/basic.html"),
    "utf8"
  );
  return <div dangerouslySetInnerHTML={{ __html: html }} />;
}
