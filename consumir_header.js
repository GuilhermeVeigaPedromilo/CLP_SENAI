document.addEventListener("DOMContentLoaded", () => {
  const placeholder = document.getElementById("header-placeholder");

  if (placeholder) {
    fetch('header.html')
      .then(response => response.text())
      .then(data => {
        placeholder.innerHTML = (new DOMParser()).parseFromString(data, "text/html").body.innerHTML;

        
        // Após carregar o header
        const btnMqtt = document.getElementById("BtnMqtt");
        if (btnMqtt) {
          btnMqtt.addEventListener("click", function(event) {
            event.preventDefault(); // impede o href de funcionar imediatamente
            postMqtt();
          });
        }
        // Agora o header foi carregado - podemos adicionar o onclick no botão
        const btnSmart = document.getElementById("BtnSmart");
        if (btnSmart) {
          btnSmart.addEventListener("click", function(event) {
            event.preventDefault(); // impede o href de funcionar imediatamente
            postLadder();
          });
        }
      })
      .catch(error => console.error("Erro ao carregar o header:", error));
  }
});

// Função postLadder separada
function postLadder() {
  fetch('/ladder', { method: 'POST' })
    .then(response => {
      console.log('Resposta do ESP32 recebida.');
      window.location.href = "https://smartladder.com.br/"; // redireciona depois do POST
    })
    .catch(error => {
      console.error('Erro:', error);
      window.location.href = "https://smartladder.com.br/"; // mesmo erro, redireciona
    });
}

// Nova função postMqtt
function postMqtt() {
  fetch('/mqtt', { method: 'POST' })
    .then(response => {
      console.log('Resposta do ESP32 recebida.');
      window.location.href = "http://clp.local/mqtt"; // redireciona depois do POST
    })
    .catch(error => {
      console.error('Erro:', error);
      window.location.href = "http://clp.local/mqtt"; // mesmo erro, redireciona
    });
}