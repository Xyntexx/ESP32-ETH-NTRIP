    document.getElementById("startSurveyBtn").addEventListener("click", function() {
        const surveyTime = document.getElementById("surveyTimeInput").value;
        const surveyAccuracy = document.getElementById("surveyAccuracyInput").value;

        // Show loading state
        const statusPanel = document.getElementById("surveyStatusPanel");
        const statusIcon = document.getElementById("surveyStatusIcon");
        const statusMessage = document.getElementById("surveyStatusMessage");
        
        statusPanel.style.display = "flex";
        statusIcon.className = "status-icon loading";
        statusMessage.textContent = "Starting survey...";

        fetch(`/startSurvey?time=${surveyTime}&accuracy=${surveyAccuracy}`)
            .then(response => {
                if (response.status === 200) {
                    return response.text();
                } else {
                    throw new Error(`Survey start failed: ${response.statusText}`);
                }
            })
            .then(data => {
                // Update to success state
                statusIcon.className = "status-icon success";
                statusIcon.innerHTML = "✓";
                statusMessage.textContent = "Survey started successfully!";
                updateStatus();
                
                // Hide status panel after 3 seconds
                setTimeout(() => {
                    statusPanel.style.display = "none";
                }, 3000);
            })
            .catch(error => {
                // Update to error state
                statusIcon.className = "status-icon error";
                statusIcon.innerHTML = "✕";
                statusMessage.textContent = error.message;
                
                // Hide status panel after 5 seconds
                setTimeout(() => {
                    statusPanel.style.display = "none";
                }, 5000);
            });
    });

    document.getElementById("stopSurveyBtn").addEventListener("click", function() {
        // Show loading state
        const statusPanel = document.getElementById("surveyStatusPanel");
        const statusIcon = document.getElementById("surveyStatusIcon");
        const statusMessage = document.getElementById("surveyStatusMessage");
        
        statusPanel.style.display = "flex";
        statusIcon.className = "status-icon loading";
        statusMessage.textContent = "Stopping survey...";

        fetch("/stopSurvey")
            .then(response => {
                if (response.status === 200) {
                    return response.text();
                } else {
                    throw new Error(`Survey stop failed: ${response.statusText}`);
                }
            })
            .then(data => {
                // Update to success state
                statusIcon.className = "status-icon success";
                statusIcon.innerHTML = "✓";
                statusMessage.textContent = "Survey stopped successfully!";
                updateStatus();
                
                // Hide status panel after 3 seconds
                setTimeout(() => {
                    statusPanel.style.display = "none";
                }, 3000);
            })
            .catch(error => {
                // Update to error state
                statusIcon.className = "status-icon error";
                statusIcon.innerHTML = "✕";
                statusMessage.textContent = error.message;
                
                // Hide status panel after 5 seconds
                setTimeout(() => {
                    statusPanel.style.display = "none";
                }, 5000);
            });
    });

    // Consolidated status update function
    function updateStatus() {
        fetch("/status")
            .then(response => response.json())
            .then(data => {
                // Update System Info - use textContent to prevent XSS
                document.getElementById("firmwareVersion").textContent = data.firmwareVersion || "--";
                document.getElementById("buildDate").textContent = data.buildDate || "--";
                document.getElementById("uptime").textContent = data.uptime || "--";

                // Update GPS Status - use textContent to prevent XSS
                document.getElementById("gpsStatus").textContent = data.gpsStatusString || "--";
                document.getElementById("gpsCoordinates").textContent =
                    `Lat: ${data.gpsLatitude || "--"}, Lon: ${data.gpsLongitude || "--"}`;
                document.getElementById("sivCard").textContent = data.gpsSiv || "--";
                document.getElementById("gpsMode").textContent = data.gpsMode || "--";

                // Update Survey Status
                const surveyContainer = document.getElementById("surveyStatusContainer");
                const surveyDetails = document.getElementById("surveyDetails");
                
                if (data.surveyInActive) {
                    surveyContainer.style.display = "block";
                    surveyContainer.className = "survey-status in-progress";
                    document.getElementById("surveyActive").textContent = "Active";
                    surveyDetails.style.display = "block";
                    document.getElementById("surveyTime").textContent = data.surveyInObservationTime || "0";
                    document.getElementById("surveyAccuracy").textContent = data.surveyInMeanAccuracy ? Math.round(data.surveyInMeanAccuracy * 100) / 100 : "-";
                } else if (data.surveyInValid) {
                    surveyContainer.style.display = "block";
                    surveyContainer.className = "survey-status completed";
                    document.getElementById("surveyActive").textContent = "Completed";
                    surveyDetails.style.display = "block";
                    document.getElementById("surveyTime").textContent = data.surveyInObservationTime || "0";
                    document.getElementById("surveyAccuracy").textContent = data.surveyInMeanAccuracy ? Math.round(data.surveyInMeanAccuracy * 100) / 100 : "-";
                } else {
                    surveyContainer.style.display = "none";
                    surveyDetails.style.display = "none";
                }

                // Update Primary NTRIP status
                const ntripConn1 = document.getElementById("ntripConnection1");
                const ntripCard1 = document.getElementById("ntripCard1");
                const ntripEnabled1 = document.getElementById("ntripEnabled1");
                
                // Always show NTRIP status regardless of enabled state
                ntripEnabled1.textContent = data.enableCaster1 ? "Enabled" : "Disabled";
                ntripEnabled1.className = data.enableCaster1 ? "status-label status-active" : "status-label status-inactive";
                
                if (data.ntripConnected1) {
                    ntripConn1.textContent = "Connected";
                    ntripConn1.className = "status-value status-active";
                    ntripCard1.textContent = "Uptime: " + (data.ntripUptime1 || "--");
                    ntripCard1.style.display = "block";
                } else {
                    ntripConn1.textContent = "Disconnected";
                    ntripConn1.className = "status-value status-inactive";
                    ntripCard1.textContent = "Uptime: --";
                    ntripCard1.style.display = "block";
                }

                // Update Secondary NTRIP status
                const ntripConn2 = document.getElementById("ntripConnection2");
                const ntripCard2 = document.getElementById("ntripCard2");
                const ntripEnabled2 = document.getElementById("ntripEnabled2");

                // Always show NTRIP status regardless of enabled state
                ntripEnabled2.textContent = data.enableCaster2 ? "Enabled" : "Disabled";
                ntripEnabled2.className = data.enableCaster2 ? "status-label status-active" : "status-label status-inactive";

                if (data.ntripConnected2) {
                    ntripConn2.textContent = "Connected";
                    ntripConn2.className = "status-value status-active";
                    ntripCard2.textContent = "Uptime: " + (data.ntripUptime2 || "--");
                    ntripCard2.style.display = "block";
                } else {
                    ntripConn2.textContent = "Disconnected";
                    ntripConn2.className = "status-value status-inactive";
                    ntripCard2.textContent = "Uptime: --";
                    ntripCard2.style.display = "block";
                }
            })
            .catch(error => console.error("Error fetching status:", error));
    }

function submitMessage() {
    setTimeout(function () {
        //document.location.reload(); 
        window.location.replace("/");
    }, 10000);
}

function updateLogs() {
    const xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            let json = JSON.parse(this.responseText);
            const logTable = document.getElementById('logTable');
            const serverTimestamp = json.timestamp;
            const currentTime = Date.now();

            // Clear existing rows
            while (logTable.firstChild) {
                logTable.removeChild(logTable.firstChild);
            }

            json.log.forEach(logEntry => {
                if (!Array.isArray(logEntry) || logEntry.length !== 2) return; // Skip invalid entries
                
                const logMillis = parseInt(logEntry[0]);
                const logText = logEntry[1];
                
                // Calculate time difference between now and server timestamp
                const timeOffset = currentTime - serverTimestamp;
                
                // Convert log millis to absolute time by adding the offset
                // Current time - (current server time - log time)
                const absoluteLogTime = new Date(currentTime - (serverTimestamp - logMillis));
                
                // Format with date and time
                const options = { 
                    year: 'numeric', 
                    month: 'short', 
                    day: 'numeric',
                    hour: '2-digit', 
                    minute: '2-digit', 
                    second: '2-digit'
                };
                const formattedDateTime = absoluteLogTime.toLocaleString(undefined, options);
                
                const row = document.createElement('tr');
                row.className = logText.includes("ERR") ? "log-error" : 
                              logText.includes("INF") ? "log-info" : "";

                const timeCell = document.createElement('td');
                timeCell.textContent = formattedDateTime;
                
                const infoCell = document.createElement('td');
                infoCell.textContent = logText;

                row.appendChild(timeCell);
                row.appendChild(infoCell);
                logTable.appendChild(row);
            });
        }
    };
    xhttp.open("GET", "/log", true);
    xhttp.send();
}

function indexStart() {
    resetLoading();
    fetchSettings();
    initOtaUpdate();
    initVersionToggles();
    initCasterToggles(); // Initialize caster toggle event listeners
    
    // Initial status update
    updateStatus();
    updateLogs();
    
    // Update status every 2 seconds
    setInterval(updateStatus, 5000);
    
    // Update logs every 5 seconds
    setInterval(updateLogs, 9950);
}

function showLoadingError(message) {
    const loadingMessage = document.getElementById('loadingMessage');
    const retryButton = document.getElementById('retryButton');
    
    loadingMessage.classList.add('loading-error');
    loadingMessage.textContent = message;
    retryButton.style.display = 'block';
}

function resetLoading() {
    const loadingMessage = document.getElementById('loadingMessage');
    const retryButton = document.getElementById('retryButton');
    
    loadingMessage.classList.remove('loading-error');
    loadingMessage.textContent = 'Loading settings...';
    retryButton.style.display = 'none';
}

function fetchSettings() {
    fetch('/getSettings')
        .then(response => {
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}: ${response.statusText}`);
            }
            return response.json();
        })
        .then(data => {
            updateFormValues(data);
        })
        .catch(error => {
            console.error('Error loading settings:', error);
            showLoadingError('Failed to load settings. Please check your connection.');
        });
}

// Add event listener for retry button
document.getElementById('retryButton').addEventListener('click', () => {
    resetLoading();
    fetchSettings();
});

function enableForm() {
    // Add error handling
    try {
        // Remove loading overlay
        const loadingOverlay = document.getElementById('loadingOverlay');
        if (loadingOverlay) {
            loadingOverlay.style.display = 'none';
        }
        
        // Enable the form
        const form = document.querySelector('.settings-form');
        if (form) {
            form.classList.add('loaded');
            form.classList.remove('form-loading');
        }
        
        // Enable all form inputs
        document.querySelectorAll('.form-group').forEach(group => {
            group.classList.remove('disabled');
            const input = group.querySelector('input');
            if (input) {
                input.disabled = false;
            }
        });
    } catch (error) {
        console.error('Error enabling form:', error);
        showLoadingError('Failed to enable form. Please refresh the page.');
    }
}

function updateCasterFields(isPrimary, enabled) {
    const suffix = isPrimary ? '1' : '2';
    const fields = [
        `casterHost${suffix}`,
        `casterPort${suffix}`,
        `rtk_mntpnt${suffix}`,
        `rtk_mntpnt_user${suffix}`,
        `rtk_mntpnt_pw${suffix}`
    ];
    
    // Handle version toggle separately
    const versionToggle = document.getElementById(`ntripVersion${suffix}`);
    if (versionToggle) {
        // Remove disabled attribute if enabled, add it if disabled
        if (enabled) {
            versionToggle.removeAttribute('disabled');
        } else {
            versionToggle.setAttribute('disabled', '');
        }
        
        // Update the switch container's appearance
        const switchContainer = versionToggle.closest('.switch-container');
        if (switchContainer) {
            if (enabled) {
                switchContainer.classList.remove('disabled');
            } else {
                switchContainer.classList.add('disabled');
            }
        }
    }
    
    // Handle other fields
    fields.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.disabled = !enabled;
            const formGroup = element.closest('.form-group');
            if (formGroup) {
                if (!enabled) {
                    formGroup.classList.add('disabled');
                } else {
                    formGroup.classList.remove('disabled');
                }
            }
        }
    });
}

// Add event listeners for caster enable toggles
function initCasterToggles() {
    const enableCaster1 = document.getElementById('enableCaster1');
    const enableCaster2 = document.getElementById('enableCaster2');
    
    if (enableCaster1) {
        enableCaster1.addEventListener('change', function() {
            updateCasterFields(true, this.checked);
        });
    }
    
    if (enableCaster2) {
        enableCaster2.addEventListener('change', function() {
            updateCasterFields(false, this.checked);
        });
    }
}

function initVersionToggles() {
    const version1Toggle = document.getElementById('ntripVersion1');
    const version2Toggle = document.getElementById('ntripVersion2');
    const user1Input = document.getElementById('rtk_mntpnt_user1');
    const user2Input = document.getElementById('rtk_mntpnt_user2');

    function updateUsernameField(toggle, userInput) {
        if (!toggle || !userInput) return;

        const isVersion2 = toggle.checked;
        userInput.disabled = !isVersion2;

        // Clear value and remove required when disabled (NTRIP 1.0 doesn't use username)
        if (!isVersion2) {
            userInput.value = '';
            userInput.removeAttribute('required');
        } else {
            userInput.setAttribute('required', 'required');
        }
    }

    if (version1Toggle) {
        version1Toggle.addEventListener('change', function() {
            console.log('Primary NTRIP version changed:', this.checked ? '2.0' : '1.0');
            updateUsernameField(this, user1Input);
        });
        // Initialize on page load
        updateUsernameField(version1Toggle, user1Input);
    }

    if (version2Toggle) {
        version2Toggle.addEventListener('change', function() {
            console.log('Secondary NTRIP version changed:', this.checked ? '2.0' : '1.0');
            updateUsernameField(this, user2Input);
        });
        // Initialize on page load
        updateUsernameField(version2Toggle, user2Input);
    }
}

function updateFormValues(data) {
    try {
        // Helper function to safely update input value
        const safeSetValue = (id, value) => {
            const element = document.getElementById(id);
            if (element) {
                if (element.type === 'checkbox') {
                    // For checkboxes, convert string 'on' or boolean true to checked
                    element.checked = value === 'on' || value === true;
                } else {
                    element.value = value || '';
                }
            }
        };

        // Update primary caster settings
        safeSetValue('enableCaster1', data.enableCaster1);
        safeSetValue('ntrip_sName', data.ntrip_sName);
        
        // Handle version settings
        safeSetValue('ntripVersion1', data.ntripVersion1 === 2 || data.ntripVersion1 === '2' || data.ntripVersion1 === true);
        safeSetValue('ntripVersion2', data.ntripVersion2 === 2 || data.ntripVersion2 === '2' || data.ntripVersion2 === true);
        
        safeSetValue('casterHost1', data.casterHost1);
        safeSetValue('casterPort1', data.casterPort1);
        safeSetValue('rtk_mntpnt1', data.rtk_mntpnt1);
        safeSetValue('rtk_mntpnt_user1', data.rtk_mntpnt_user1);
        safeSetValue('rtk_mntpnt_pw1', data.rtk_mntpnt_pw1);
        
        // Update secondary caster settings
        safeSetValue('enableCaster2', data.enableCaster2);
        safeSetValue('casterHost2', data.casterHost2);
        safeSetValue('casterPort2', data.casterPort2);
        safeSetValue('rtk_mntpnt2', data.rtk_mntpnt2);
        safeSetValue('rtk_mntpnt_user2', data.rtk_mntpnt_user2);
        safeSetValue('rtk_mntpnt_pw2', data.rtk_mntpnt_pw2);
        
        // Update RTCM checks setting
        safeSetValue('enableRtcmChecks', data.rtcmChk);
        
        // Update ECEF coordinates
        if (data.ecefX !== undefined) {
            const ecefX = parseFloat(data.ecefX) / 100;
            safeSetValue('ecefX', ecefX);
        }
        if (data.ecefY !== undefined) {
            const ecefY = parseFloat(data.ecefY) / 100;
            safeSetValue('ecefY', ecefY);
        }
        if (data.ecefZ !== undefined) {
            const ecefZ = parseFloat(data.ecefZ) / 100;
            safeSetValue('ecefZ', ecefZ);
        }
        
        // Update field states based on enable settings
        updateCasterFields(true, !!data.enableCaster1);
        updateCasterFields(false, !!data.enableCaster2);

        // Trigger username field updates based on loaded NTRIP versions
        // This ensures username fields are properly disabled for NTRIP 1.0
        const version1Toggle = document.getElementById('ntripVersion1');
        const version2Toggle = document.getElementById('ntripVersion2');
        if (version1Toggle) version1Toggle.dispatchEvent(new Event('change'));
        if (version2Toggle) version2Toggle.dispatchEvent(new Event('change'));

        // Enable the form after data is loaded
        enableForm();
    } catch (error) {
        console.error('Error updating form values:', error);
        showLoadingError('Failed to update form values. Please try again.');
    }
}

// Add form submission handler
document.querySelector('.settings-form').addEventListener('submit', function(e) {
    e.preventDefault();

    // Create FormData object
    const formData = new FormData();
    
    // Add all basic fields
    formData.append('ntrip_sName', document.getElementById('ntrip_sName').value);
    formData.append('ntripVersion1', document.getElementById('ntripVersion1').checked ? '2' : '1'); // Convert boolean to version number
    formData.append('ntripVersion2', document.getElementById('ntripVersion2').checked ? '2' : '1'); // Convert boolean to version number
    
    // Add enable flags for both casters and RTCM checks
    formData.append('enableCaster1', document.getElementById('enableCaster1').checked ? 'on' : '');
    formData.append('enableCaster2', document.getElementById('enableCaster2').checked ? 'on' : '');
    formData.append('rtcmChk', document.getElementById('enableRtcmChecks').checked ? 'on' : '');
    
    // Add ECEF coordinates - convert from centimeters with 0.1mm precision to 0.1mm precision integers
    // Format: -1234.5678 cm should be sent as -12345678 (0.1mm units)
    const ecefX = parseFloat(document.getElementById('ecefX').value);
    const ecefY = parseFloat(document.getElementById('ecefY').value);
    const ecefZ = parseFloat(document.getElementById('ecefZ').value);
    
    formData.append('ecefX', Math.round(ecefX * 100).toString());
    formData.append('ecefY', Math.round(ecefY * 100).toString());
    formData.append('ecefZ', Math.round(ecefZ * 100).toString());
    
    // Add primary caster fields if enabled
    if (document.getElementById('enableCaster1').checked) {
        formData.append('casterHost1', document.getElementById('casterHost1').value);
        formData.append('casterPort1', document.getElementById('casterPort1').value);
        formData.append('rtk_mntpnt1', document.getElementById('rtk_mntpnt1').value);
        formData.append('rtk_mntpnt_user1', document.getElementById('rtk_mntpnt_user1').value);
        formData.append('rtk_mntpnt_pw1', document.getElementById('rtk_mntpnt_pw1').value);
    }
    
    // Add secondary caster fields if enabled
    if (document.getElementById('enableCaster2').checked) {
        formData.append('casterHost2', document.getElementById('casterHost2').value);
        formData.append('casterPort2', document.getElementById('casterPort2').value);
        formData.append('rtk_mntpnt2', document.getElementById('rtk_mntpnt2').value);
        formData.append('rtk_mntpnt_user2', document.getElementById('rtk_mntpnt_user2').value);
        formData.append('rtk_mntpnt_pw2', document.getElementById('rtk_mntpnt_pw2').value);
    }
    
    // Show loading overlay
    document.getElementById('loadingOverlay').style.display = 'block';
    document.getElementById('loadingMessage').textContent = 'Saving settings...';
    
    // Send the form data
    fetch('/applySettings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams(formData)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`Server returned ${response.status}: ${response.statusText}`);
        }
        return response.text();
    })
    .then(data => {
        document.getElementById('loadingMessage').textContent = 'Settings saved successfully!';
        submitMessage();
    })
    .catch(error => {
        console.error('Error saving settings:', error);
        showLoadingError('Failed to save settings. Please try again.');
    });
});

// Add this function to handle password toggling
function togglePassword(inputId) {
    const input = document.getElementById(inputId);
    if (input) {
        if (input.type === 'password') {
            input.type = 'text';
        } else {
            input.type = 'password';
        }
    }
}

function initOtaUpdate() {
    const form = document.getElementById('uploadForm');
    const progressContainer = document.querySelector('.progress-container');
    const progressFill = document.querySelector('.progress-fill');
    const progressText = document.querySelector('.progress-text');
    const submitButton = form.querySelector('button[type="submit"]');
    const rebootOverlay = document.querySelector('.reboot-overlay');
    const rebootMessage = document.getElementById('rebootMessage');

    if (form) {
        form.onsubmit = async (e) => {
            e.preventDefault();
            
            const fileField = document.querySelector('input[type="file"]');
            
            if (!fileField.files[0]) {
                alert('Please select a file first!');
                return;
            }

            // Disable form elements during upload
            submitButton.disabled = true;
            fileField.disabled = true;
            progressContainer.style.display = 'block';
            progressFill.style.width = '0%';
            progressText.textContent = '0%';

            try {
                const xhr = new XMLHttpRequest();
                xhr.open('POST', '/update', true);

                // Track upload progress
                xhr.upload.onprogress = (e) => {
                    if (e.lengthComputable) {
                        const percentComplete = (e.loaded / e.total) * 100;
                        progressFill.style.width = percentComplete + '%';
                        progressText.textContent = Math.round(percentComplete) + '%';
                    }
                };

                // Handle response
                xhr.onload = function() {
                    if (xhr.status === 200) {
                        progressFill.style.width = '100%';
                        progressText.textContent = '100%';
                        
                        // Show reboot overlay with updating message
                        rebootMessage.textContent = 'Updating firmware...';
                        rebootOverlay.style.display = 'flex';
                        
                        // Attempt to reload after 20 seconds
                        setTimeout(() => {
                            window.location.reload();
                        }, 10000);
                    } else {
                        throw new Error('Update failed: ' + xhr.statusText);
                    }
                };

                xhr.onerror = function() {
                    throw new Error('Network error occurred');
                };

                // Create FormData and append file
                const formData = new FormData();
                formData.append('update', fileField.files[0], 'firmware.bin');

                // Send the form data
                xhr.send(formData);

            } catch (error) {
                console.error('Error:', error);
                alert('Error uploading file: ' + error.message);
                // Re-enable form elements on error
                submitButton.disabled = false;
                fileField.disabled = false;
                progressContainer.style.display = 'none';
                progressFill.style.width = '0%';
                progressText.textContent = '0%';
            }
        };

        // Add file input change handler to show selected file
        const fileInput = form.querySelector('input[type="file"]');
        fileInput.addEventListener('change', () => {
            const fileName = fileInput.files[0]?.name || 'No file selected';
            const small = fileInput.parentElement.querySelector('small');
            small.textContent = fileName;
            
            // Reset progress if new file selected
            progressContainer.style.display = 'none';
            progressFill.style.width = '0%';
            progressText.textContent = '0%';
        });
    }
}


