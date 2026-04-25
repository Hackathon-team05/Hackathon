import fs from "fs";
import path from "path";

export default function AssignPage() {
  const html = fs.readFileSync(
    path.join(process.cwd(), "src/data/assign.html"),
    "utf8"
  );
  return <div dangerouslySetInnerHTML={{ __html: html }} />;
}
