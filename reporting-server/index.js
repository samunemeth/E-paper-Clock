import express from "express";
import serveIndex from "serve-index";
import path from "path";
import fs from "fs/promises";
import { fileURLToPath } from 'url';

// Settings.
const port = 12891;
const dataFileExtension = "log";

// Get absolute paths.
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Express setup.
const app = express();
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

    // Parse incoming data.
    const data = req.body;
    console.log(`Received the following data: ${JSON.stringify(data)}`);

    // Format needed data.
    const timestamp = (new Date()).toISOString();
    const targetFilePath = path.join(__dirname, `./data/${data.uuid}.${dataFileExtension}`);

    // Create file if not yet present.
    try {
        await fs.access(targetFilePath);
    } catch {
        await fs.writeFile(targetFilePath, "timestamp; batteryLevel; bootNum; currentMode; timeShift; timeShiftAverage; timeShiftSamples, syncDuration, wifiStrength\n", "utf8");
    }

    // Append data to file.
    await fs.appendFile(targetFilePath, `${timestamp}; ${data.batteryLevel}; ${data.bootNum}; ${data.currentMode}; ${data.timeShift}; ${data.timeShiftAverage}; ${data.timeShiftSamples}, ${data.syncDuration}, ${data.wifiStrength}\n`, "utf8");

    // Send and OK response.
    res.sendStatus(200);
});

// Create the date folder if needed.
try { await fs.mkdir("./data"); } catch (error) { };

// Start the server.
app.listen(port, () => {
    console.log(`App listening on port ${port}`);
});