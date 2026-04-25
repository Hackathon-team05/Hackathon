import fs from "fs";
import path from "path";

export default function SchedulePage() {
  const html = fs.readFileSync(
    path.join(process.cwd(), "src/data/schedule.html"),
    "utf8"
  );
  return <div dangerouslySetInnerHTML={{ __html: html }} />;
}
