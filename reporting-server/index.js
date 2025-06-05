import express from "express";
import serveIndex from "serve-index";
import path from "path";
import fs from "fs/promises";
import { fileURLToPath } from 'url';

const app = express();
const port = 12891;

// Get absolute paths.
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Use json formatting to get reported data.
app.use(express.json());

// Serve the static main page.
const publicPath = path.join(__dirname, "./public");
app.use("/", express.static(publicPath));

// Server the collected data.
const dataPath = path.join(__dirname, "./data");
app.use("/data", express.static(dataPath));
app.use("/data", serveIndex(dataPath));
app.use("/data", (req, res, next) => {
    res.set("Content-Disposition", "inline");
    res.set("Content-Type", "text/plain");
    next();
});


// Get the data from the reports.
app.post("/report", async (req, res) => {
    const data = req.body;
    console.log(`Received the following data: ${JSON.stringify(data)}`);
    const timestamp = (new Date()).toISOString();
    await fs.appendFile(`./data/${data.uuid}.log`, `${timestamp}; ${data.batteryLevel}\n`, "utf8");
    res.sendStatus(200);
});

// Create the date folder if needed.
try { await fs.mkdir("./data"); } catch (error) { };

// Start the server.
app.listen(port, () => {
    console.log(`App listening on port ${port}`);
});