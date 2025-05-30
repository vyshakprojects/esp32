// --- Global Functions ---

async function fetchData(url, method = 'GET', data = null) {
    const options = {
        method: method,
        headers: {
            'Content-Type': 'application/json'
        }
    };
    if (data) {
        options.body = JSON.stringify(data);
    }
    try {
        const response = await fetch(url, options);
        if (!response.ok) {
            const errorText = await response.text();
            throw new Error(`HTTP error! status: ${response.status}, message: ${errorText}`);
        }
        return response.json();
    } catch (error) {
        console.error('Fetch error:', error);
        alert('Error: ' + error.message);
        return null;
    }
}

// --- Home Page Functions ---

async function loadButtons() {
    const buttonsContainer = document.getElementById('buttons-container');
    buttonsContainer.innerHTML = 'Loading buttons...';
    const buttons = await fetchData('/api/buttons');
    if (buttons) {
        buttonsContainer.innerHTML = ''; // Clear loading message
        if (buttons.length === 0) {
            buttonsContainer.innerHTML = '<p>No buttons configured. Go to <a href="/settings.html">Settings</a> to add some.</p>';
            return;
        }
        buttons.forEach(button => {
            const buttonCard = document.createElement('div');
            buttonCard.className = 'button-card';
            buttonCard.innerHTML = `
                <h3>${button.name} (GPIO ${button.gpioPin})</h3>
                <button id="btn-${button.id}" class="${button.state ? 'on' : 'off'}" onclick="toggleButton('${button.id}', ${button.gpioPin}, ${button.state})">
                    ${button.state ? 'ON' : 'OFF'}
                </button>
            `;
            buttonsContainer.appendChild(buttonCard);
        });
    } else {
        buttonsContainer.innerHTML = '<p>Failed to load buttons.</p>';
    }
}

async function toggleButton(buttonId, gpioPin, currentState) {
    const newState = !currentState;
    const buttonElement = document.getElementById(`btn-${buttonId}`);
    if (!buttonElement) return;

    // Optimistic UI update
    buttonElement.classList.remove(currentState ? 'on' : 'off');
    buttonElement.classList.add(newState ? 'on' : 'off');
    buttonElement.textContent = newState ? 'ON' : 'OFF';
    buttonElement.onclick = () => toggleButton(buttonId, gpioPin, newState); // Update handler immediately

    const success = await fetchData('/api/buttons', 'POST', { id: buttonId, state: newState });
    if (!success) {
        // Revert if API call fails
        buttonElement.classList.remove(newState ? 'on' : 'off');
        buttonElement.classList.add(currentState ? 'on' : 'off');
        buttonElement.textContent = currentState ? 'ON' : 'OFF';
        buttonElement.onclick = () => toggleButton(buttonId, gpioPin, currentState); // Revert handler
        alert('Failed to toggle button state.');
    }
    // No need to reload all buttons, just update history (if needed)
}


// --- Settings Page Functions ---

async function populateAvailableGPIOPins(selectElementId, currentPin = null) {
    const selectElement = document.getElementById(selectElementId);
    selectElement.innerHTML = '<option value="">Select GPIO Pin</option>';

    const availablePins = await fetchData('/api/gpio/available');
    if (availablePins && availablePins.length > 0) {
        availablePins.forEach(pin => {
            const option = document.createElement('option');
            option.value = pin;
            option.textContent = `GPIO ${pin}`;
            if (currentPin !== null && pin === currentPin) {
                option.selected = true; // Pre-select if editing
            }
            selectElement.appendChild(option);
        });
    } else {
        console.warn('No available GPIO pins or failed to fetch.');
        const option = document.createElement('option');
        option.value = "";
        option.textContent = "No pins available";
        option.disabled = true;
        selectElement.appendChild(option);
    }
}

async function handleAddButton(event) {
    event.preventDefault();
    const name = document.getElementById('new-button-name').value.trim();
    const gpioPin = parseInt(document.getElementById('new-button-gpio').value);

    if (!name || isNaN(gpioPin)) {
        alert('Please enter a valid name and select a GPIO pin.');
        return;
    }

    const newButton = { name, gpioPin };
    const result = await fetchData('/api/settings/buttons', 'POST', newButton);
    if (result && result.success) {
        alert('Button added successfully!');
        document.getElementById('add-button-form').reset();
        loadSettingsButtons(); // Reload list
        populateAvailableGPIOPins('new-button-gpio'); // Refresh available pins
    } else {
        alert('Failed to add button: ' + (result ? result.message : 'Unknown error'));
    }
}

async function loadSettingsButtons() {
    const listContainer = document.getElementById('existing-buttons-list');
    listContainer.innerHTML = 'Loading existing buttons...';
    const buttons = await fetchData('/api/settings/buttons');
    if (buttons) {
        listContainer.innerHTML = '';
        if (buttons.length === 0) {
            listContainer.innerHTML = '<p>No buttons configured yet.</p>';
            return;
        }
        buttons.forEach(button => {
            const buttonItem = document.createElement('div');
            buttonItem.className = 'existing-button-item';
            buttonItem.innerHTML = `
                <span>${button.name} (GPIO ${button.gpioPin})</span>
                <div>
                    <button class="edit" onclick="editButton('${button.id}', '${button.name}', ${button.gpioPin})">Edit</button>
                    <button class="delete" onclick="deleteButton('${button.id}')">Delete</button>
                </div>
            `;
            listContainer.appendChild(buttonItem);
        });
    } else {
        listContainer.innerHTML = '<p>Failed to load existing buttons.</p>';
    }
}

async function editButton(id, currentName, currentPin) {
    const newName = prompt('Enter new name for ' + currentName + ':', currentName);
    if (newName === null || newName.trim() === '') return;

    const editFormHtml = `
        <h3>Edit Button: ${currentName}</h3>
        <label for="edit-name-${id}">Name:</label>
        <input type="text" id="edit-name-${id}" value="${newName}" required><br>
        <label for="edit-gpio-${id}">GPIO Pin:</label>
        <select id="edit-gpio-${id}" required></select><br>
        <button id="save-edit-btn-${id}">Save Changes</button>
        <button onclick="loadSettingsButtons()">Cancel</button>
    `;

    // Find the item and replace it with the edit form
    const itemToEdit = document.querySelector(`.existing-button-item div:has(button[onclick="editButton('${id}', '${currentName}', ${currentPin})"])`).parentNode;
    const originalContent = itemToEdit.innerHTML; // Store for cancel
    itemToEdit.innerHTML = editFormHtml;

    // Populate the GPIO dropdown for editing
    await populateAvailableGPIOPins(`edit-gpio-${id}`, currentPin);

    document.getElementById(`save-edit-btn-${id}`).addEventListener('click', async () => {
        const editedName = document.getElementById(`edit-name-${id}`).value.trim();
        const editedPin = parseInt(document.getElementById(`edit-gpio-${id}`).value);

        if (!editedName || isNaN(editedPin)) {
            alert('Please enter a valid name and select a GPIO pin.');
            return;
        }

        const updatedButton = { id, name: editedName, gpioPin: editedPin };
        const result = await fetchData('/api/settings/buttons', 'PUT', updatedButton);
        if (result && result.success) {
            alert('Button updated successfully!');
            loadSettingsButtons(); // Reload the list
        } else {
            alert('Failed to update button: ' + (result ? result.message : 'Unknown error'));
            itemToEdit.innerHTML = originalContent; // Revert if failed
        }
    });
}

async function deleteButton(id) {
    if (!confirm('Are you sure you want to delete this button?')) {
        return;
    }
    const result = await fetchData('/api/settings/buttons', 'DELETE', { id });
    if (result && result.success) {
        alert('Button deleted successfully!');
        loadSettingsButtons(); // Reload list
        populateAvailableGPIOPins('new-button-gpio'); // Refresh available pins
    } else {
        alert('Failed to delete button: ' + (result ? result.message : 'Unknown error'));
    }
}

// --- ESP32 Status Page Functions ---

async function loadESP32Status() {
    const statusDiv = document.getElementById('status-info');
    statusDiv.innerHTML = 'Loading ESP32 status...';
    const status = await fetchData('/api/status');
    if (status) {
        document.getElementById('uptime').textContent = formatUptime(status.uptime);
        document.getElementById('free-heap').textContent = status.freeHeap;
        document.getElementById('chip-id').textContent = status.chipId;
        document.getElementById('flash-size').textContent = status.flashSize;
        document.getElementById('cpu-freq').textContent = status.cpuFreq;
        document.getElementById('sdk-version').textContent = status.sdkVersion;
        document.getElementById('wifi-rssi').textContent = status.rssi;
        document.getElementById('local-ip').textContent = status.localIP;
    } else {
        statusDiv.innerHTML = '<p>Failed to load ESP32 status.</p>';
    }
}

function formatUptime(ms) {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    const s = seconds % 60;
    const m = minutes % 60;
    const h = hours % 24;

    let uptimeString = '';
    if (days > 0) uptimeString += `${days}d `;
    if (h > 0) uptimeString += `${h}h `;
    if (m > 0) uptimeString += `${m}m `;
    uptimeString += `${s}s`;
    return uptimeString.trim();
}

// --- History Page Functions ---

async function loadHistory() {
    const historyDiv = document.getElementById('history-log');
    historyDiv.innerHTML = 'Loading history...';
    const history = await fetchData('/api/history');
    if (history) {
        historyDiv.innerHTML = '';
        if (history.length === 0) {
            historyDiv.innerHTML = '<p>No history entries yet.</p>';
            return;
        }
        history.reverse().forEach(entry => { // Show most recent first
            const historyEntry = document.createElement('div');
            historyEntry.className = 'history-entry';
            historyEntry.innerHTML = `
                <span>${entry.timestamp}</span>
                ${entry.event}
            `;
            historyDiv.appendChild(historyEntry);
        });
    } else {
        historyDiv.innerHTML = '<p>Failed to load history.</p>';
    }
}