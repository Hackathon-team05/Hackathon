import fs from "fs";
import path from "path";

export default function NonfuncPage() {
  const html = fs.readFileSync(
    path.join(process.cwd(), "src/data/nonfunc.html"),
    "utf8"
  );
  return <div dangerouslySetInnerHTML={{ __html: html }} />;
}
