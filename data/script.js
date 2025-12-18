// @ts-nocheck
const tempEl = document.getElementById('temp');
const humEl = document.getElementById('hum');
const statusBox = document.getElementById('relayStatus');
const targetInput = document.getElementById('targetTemp');
const btnSalvarTemp = document.getElementById('btnSalvarTemp');

// Elementos do Timer
const timerSectionSetup = document.getElementById('timer-setup');
const timerSectionRunning = document.getElementById('timer-running');
const timerInput = document.getElementById('timerMinutes');
const timerCountdown = document.getElementById('timerCountdown');
const btnStartTimer = document.getElementById('btnStartTimer');
const btnStopTimer = document.getElementById('btnStopTimer');

let isUpdating = false;

let updateInterval = 0;

startUpdateLoop();
document.getElementById('form-temp')?.addEventListener('submit', salvarTemperatura);
btnStartTimer?.addEventListener('click', iniciarTimer);
btnStopTimer?.addEventListener('click', pararTimer);

function startUpdateLoop() {
    atualizarDados();
    updateInterval = setInterval(atualizarDados, 2000);
}

/**
 * @param {number} seconds
 */
function formatTime(seconds) {
    const m = Math.floor(seconds / 60).toString().padStart(2, '0');
    const s = (seconds % 60).toString().padStart(2, '0');
    return `${m}:${s}`;
}

async function atualizarDados() {
    if (isUpdating) return;

    try {
        const res = await fetch('/status');
        if (!res.ok) throw new Error('Erro na rede');
        const data = await res.json();

        // Atualiza Sensores
        tempEl.innerText = data.temperatura.toFixed(1);
        humEl.innerText = data.humidade.toFixed(0);

        // Atualiza Status Visual
        if (data.status) {
            statusBox.textContent = 'Ventilador LIGADO';
            statusBox.className = 'status-box on';
        } else {
            statusBox.textContent = 'Ventilador DESLIGADO';
            statusBox.className = 'status-box off';
        }

        // Atualiza Input de Temperatura
        if (document.activeElement !== targetInput && targetInput.value != data.temperaturaAlvo) {
            targetInput.value = data.temperaturaAlvo;
        }
        
        if (data.timerAtivo) {
            timerSectionSetup.style.display = 'none';
            timerSectionRunning.style.display = 'block';
            timerCountdown.innerText = formatTime(data.timerRestante);
        } else {
            timerSectionSetup.style.display = 'block';
            timerSectionRunning.style.display = 'none';
            
            // Preenche o input com o último valor salvo
            if (document.activeElement !== timerInput && data.ultimoTimer) {
                timerInput.value = data.ultimoTimer;
            }
        }

    } catch (error) {
        console.error("Erro:", error);
    }
}

/**
 * @param {Event} e
 */
async function salvarTemperatura(e) {
    e.preventDefault();
    handleRequest(btnSalvarTemp, `/set?target=${targetInput.value}`);
}

async function iniciarTimer() {
    const min = timerInput.value;
    handleRequest(btnStartTimer, `/timer?cmd=start&min=${min}`);
}

async function pararTimer() {
    handleRequest(btnStopTimer, `/timer?cmd=stop`);
}

/**
 * @param {HTMLElement | null} btn
 * @param {RequestInfo | URL} url
 */
async function handleRequest(btn, url) {
    clearInterval(updateInterval); // Pausa atualização
    isUpdating = true;
    
    const textoOriginal = btn.innerText;
    btn.innerText = "...";
    btn.disabled = true;

    try {
        await fetch(url);
    } catch (error) {
        alert("Erro de comunicação com ESP32");
    } finally {
        setTimeout(() => {
            btn.innerText = textoOriginal;
            btn.disabled = false;
            isUpdating = false;
            atualizarDados();
            startUpdateLoop();
        }, 1000);
    }
}