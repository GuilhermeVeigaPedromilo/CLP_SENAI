document.addEventListener("DOMContentLoaded", () => {
    const placeholder = document.getElementById("header-placeholder");
  
    if (placeholder) {
      // Calcula caminho relativo da página até a raiz
      const pathDepth = window.location.pathname.split('/').length - 1;
      const basePath = '../'.repeat(pathDepth - 1); // -1 porque a primeira parte é ""
  
      const headerPath = `${basePath}src/components/header.html`;
  
      fetch(headerPath)
        .then(response => response.text())
        .then(data => {
          // Corrige as imagens do header dinamicamente
          const parser = new DOMParser();
          const doc = parser.parseFromString(data, "text/html");
  
          // Corrige todas as imagens que começam com "./src/"
          doc.querySelectorAll('img').forEach(img => {
            const src = img.getAttribute('src');
            if (src.startsWith('./src/') || src.startsWith('src/')) {
              img.setAttribute('src', basePath + src.replace(/^\.?\/*/, ''));
            }
          });
  
          placeholder.innerHTML = doc.body.innerHTML;
        })
        .catch(error => console.error("Erro ao carregar o header:", error));
    }
  });  