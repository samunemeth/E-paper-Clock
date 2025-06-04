const express = require('express');
const fs = require('fs/promises');
const app = express();
const port = 12891;

const path = require('path');

app.use(express.json());

// Serve the static main page.
app.use('/', express.static(path.join(__dirname, 'public')))

// Get the data from the reports.
app.post('/report', async (req, res) => {
    const data = req.body;
    console.log(`Received the following data: ${JSON.stringify(data)}`);
    const timestamp = (new Date()).toISOString();
    try { await fs.mkdir('./data'); } catch (error) { };
    await fs.appendFile(`./data/${data.uuid}.txt`, `${timestamp}; ${data.batteryLevel}\n`, 'utf8');
    res.sendStatus(200);
});

// Start the server.
app.listen(port, () => {
    console.log(`App listening on port ${port}`);
});