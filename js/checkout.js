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
    // --- 1. LOGIKA PRO PŘIHLÁŠENÉHO UŽIVATELE ---
    const userId = localStorage.getItem('userId');

    if (userId && userId !== 'guest') {
        try {
            const response = await fetch(`profile.php?user_id=${userId}`);
            const data = await response.json();

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

    // --- 2. NOVÁ LOGIKA PRO TLAČÍTKA DOPRAVY (DLAŽDICE) ---
    const deliveryRadios = document.querySelectorAll('input[name="delivery_method"]');
    const pickupContainer = document.getElementById('pickup-container');
    const pickupPointId = document.getElementById('pickup-point-id');
    const btnOpenWidget = document.getElementById('open-widget-btn');
    const selectedBranchText = document.getElementById('selected-branch');

    const pickupMethods = ['zbox', 'balikovna', 'alzabox', 'pplbox'];

    function handleDeliveryChange() {
        const selectedRadio = document.querySelector('input[name="delivery_method"]:checked');
        if (!selectedRadio) return;
        
        const selectedMethod = selectedRadio.value;

        if (pickupMethods.includes(selectedMethod)) {
            if (pickupContainer) pickupContainer.style.display = 'block';
            if (pickupPointId) pickupPointId.setAttribute('required', 'required'); 
            
            if (btnOpenWidget) {
                if (selectedMethod === 'zbox') btnOpenWidget.innerText = "Vybrat Z-BOX / Pobočku na mapě";
                if (selectedMethod === 'balikovna') btnOpenWidget.innerText = "Vybrat Balíkovnu na mapě";
                if (selectedMethod === 'alzabox') btnOpenWidget.innerText = "Vybrat AlzaBox na mapě";
                if (selectedMethod === 'pplbox') btnOpenWidget.innerText = "Vybrat PPL Parcelbox na mapě";
            }
            
        } else {
            if (pickupContainer) pickupContainer.style.display = 'none';
            if (pickupPointId) {
                pickupPointId.removeAttribute('required');
                pickupPointId.value = ""; 
            }
            if (selectedBranchText) selectedBranchText.innerText = "Zatím nevybráno";
        }
    }

    deliveryRadios.forEach(radio => {
        radio.addEventListener('change', handleDeliveryChange);
    });

    // Otevření Widgetu Zásilkovny
    if (btnOpenWidget) {
        btnOpenWidget.addEventListener('click', () => {
            const packetaApiKey = "VÁŠ_API_KLÍČ_ZE_ZÁSILKOVNY";
            
            const packetaOptions = {
                country: "cz",
                language: "cs",
                valueFormat: "ID"
            };

            Packeta.Widget.pick(packetaApiKey, function(pickupPoint) {
                if (pickupPoint != null) {
                    selectedBranchText.innerText = "Vybráno: " + pickupPoint.name + " (" + pickupPoint.id + ")";
                    pickupPointId.value = pickupPoint.id;
                } else {
                    if (!pickupPointId.value) {
                        selectedBranchText.innerText = "Nevybrali jste žádnou pobočku.";
                    }
                }
            }, packetaOptions);
        });
    }
});

// --- 3. ODESLÁNÍ OBJEDNÁVKY ---
document.getElementById('checkout-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    
    if (cart.length === 0) {
        alert("Nelze odeslat prázdnou objednávku!");
        return;
    }

    // Kontrola, zda je vybrán způsob dopravy
    const deliveryMethodInput = document.querySelector('input[name="delivery_method"]:checked');
    if (!deliveryMethodInput) {
        alert("Prosím, vyberte způsob doručení.");
        return;
    }

    const statusMessage = document.getElementById('status-message');
    if (statusMessage) {
        statusMessage.style.color = "#777";
        statusMessage.textContent = "Zpracovávám...";
    }

    // Vytvoření odesílaného objektu
    const formData = {
        user_id: localStorage.getItem('userId') || 'guest',
        name: document.getElementById('name').value,
        street: document.getElementById('street').value,
        city: document.getElementById('city').value,
        zip: document.getElementById('zip').value,
        phone: document.getElementById('phone').value,
        email: document.getElementById('email').value,
        
        delivery_method: deliveryMethodInput.value,           
        
        pickup_point_id: document.getElementById('pickup-point-id') ? document.getElementById('pickup-point-id').value : "", 
        
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
            if (statusMessage) {
                statusMessage.style.color = "#2e7d32";
                statusMessage.textContent = "Objednávka byla úspěšně odeslána!";
            }
            
            localStorage.removeItem('cart');
            setTimeout(() => {
                window.location.href = "index.html";
            }, 2000);
        } else {
            if (statusMessage) {
                statusMessage.style.color = "#d32f2f";
                statusMessage.textContent = result.message || "Nastala chyba při ukládání.";
            }
        }
    } catch (error) {
        console.error("Chyba při odesílání:", error);
        if (statusMessage) {
            statusMessage.style.color = "#d32f2f";
            statusMessage.textContent = "Kritická chyba při komunikaci se serverem.";
        }
    }
});