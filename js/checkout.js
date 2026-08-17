const cart = JSON.parse(localStorage.getItem('cart')) || [];
const cartContainer = document.getElementById('cart-items-container');
const cartTotal = document.getElementById('cart-total');

let total = 0;

if (cart.length === 0) {
    cartContainer.innerHTML = "<p style='color: #777; padding: 1rem 0;'>Váš košík je prázdný.</p>";
} else {
    cart.forEach(item => {
        const priceString = String(item.price).replace(/[^\d]/g, '');
        const priceNum = parseInt(priceString) || 0; 
        const itemTotal = priceNum * item.quantity;
        total += itemTotal;

        cartContainer.innerHTML += `
            <div class="cart-item">
                <div>
                    <strong style="color: #111;">${item.name}</strong><br>
                    <small style="color: #777;">Množství: ${item.quantity}x</small>
                </div>
                <div>
                    <strong>${itemTotal} Kč</strong>
                </div>
            </div>
        `;
    });
    cartTotal.textContent = `Celkem k úhradě: ${total} Kč`;
}

document.addEventListener('DOMContentLoaded', async () => {
    // 1. Zjistíme ID přihlášeného uživatele z localStorage
    const userId = localStorage.getItem('userId');

    // 2. Pokud je uživatel přihlášen (a není to host), stáhneme jeho data
    if (userId && userId !== 'guest') {
        try {
            // ZMĚNA: Přidán parametr user_id do URL, aby databáze věděla, koho hledat
            const response = await fetch(`profile.php?user_id=${userId}`);
            const data = await response.json();

            // 3. Předvyplnění formuláře
            if (!data.error) {
                document.getElementById('name').value = data.name || '';
                document.getElementById('street').value = data.street || '';
                document.getElementById('city').value = data.city || '';
                document.getElementById('zip').value = data.zip || '';
                document.getElementById('phone').value = data.phone || '';
                document.getElementById('email').value = data.email || '';
            }
        } catch (error) {
            console.error("Chyba při načítání dat:", error);
        }
    }
});

document.getElementById('checkout-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    
    if (cart.length === 0) {
        alert("Nelze odeslat prázdnou objednávku!");
        return;
    }

    const statusMessage = document.getElementById('status-message');
    statusMessage.style.color = "#777";
    statusMessage.textContent = "Zpracovávám...";

    // ZMĚNA: user_id se nyní načítá z localStorage (fallback 'guest' místo '1')
    const formData = {
        user_id: localStorage.getItem('userId') || 'guest',
        name: document.getElementById('name').value,
        street: document.getElementById('street').value,
        city: document.getElementById('city').value,
        zip: document.getElementById('zip').value,
        phone: document.getElementById('phone').value,
        email: document.getElementById('email').value,
        cart_items: cart, 
        total: total      
    };

    try {
        const response = await fetch('save_order.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(formData)
        });
        
        const result = await response.json();

        if (result.success) {
            statusMessage.style.color = "#2e7d32";
            statusMessage.textContent = "Objednávka byla úspěšně odeslána!";
            
            localStorage.removeItem('cart');
            setTimeout(() => {
                window.location.href = "index.html";
            }, 2000);
        } else {
            statusMessage.style.color = "#d32f2f";
            statusMessage.textContent = result.message || "Nastala chyba při ukládání.";
        }
    } catch (error) {
        console.error("Chyba při odesílání:", error);
        statusMessage.style.color = "#d32f2f";
        statusMessage.textContent = "Kritická chyba při komunikaci se serverem.";
    }
});