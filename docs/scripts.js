const flashEl   = document.getElementById("flashEl");   // <esp-web-install-button>
const flashBtn  = document.getElementById("flashBtn");  // slotted button
const flashHint = document.getElementById("flashHint");
const grid      = document.getElementById("boards");

let selected = null;

function setActiveCard(key) {
  const cards = grid.querySelectorAll(".card");
  for (const el of cards) {
    const on = !!key && el.dataset.key === key;
    el.classList.toggle("active", on);
    el.setAttribute("aria-selected", on ? "true" : "false");
  }
}

function setDisabled(state) {
  if (state) {
    flashBtn.disabled = true;
    flashBtn.setAttribute("disabled", "");
  } else {
    flashBtn.disabled = false;
    flashBtn.removeAttribute("disabled");
  }
}

function updateUI() {
  if (!selected) {
    flashEl.removeAttribute("manifest");
    setDisabled(true);
    flashHint.textContent = "请先选择上面的开发板。";
    setActiveCard(null);
    return;
  }
  const manifestUrl = `./manifests/${selected}.json`;
  flashEl.setAttribute("manifest", manifestUrl);
  setDisabled(false);
  flashHint.textContent = "已准备就绪：点击烧录。";
  setActiveCard(selected);
}

// 点击开发板
grid.addEventListener("click", (e) => {
  const el = e.target.closest(".card");
  if (!el) return;
  selected = el.dataset.key || null;
  updateUI();
});

// 初始化
updateUI();
