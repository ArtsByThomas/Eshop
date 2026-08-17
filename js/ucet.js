document.getElementById('phone').addEventListener('input', function (e) {
    this.value = this.value.replace(/[^0-9+\s]/g, '');
});

document.addEventListener('DOMContentLoaded', async () => {
    const userId = localStorage.getItem('userId') || 'guest';
    
    // Načtení profilu
    try {
        const response = await fetch(`profile.php?user_id=${userId}`);
        const data = await response.json();

        if (data && !data.error) {
            document.getElementById('name').value = data.name || '';
            document.getElementById('street').value = data.street || '';
            document.getElementById('zip').value = data.zip || '';
            document.getElementById('city').value = data.city || '';
            document.getElementById('phone').value = data.phone || '';
            document.getElementById('email').value = data.email || '';
        }
    } catch (error) {
        console.error("Nepodařilo se načíst data profilu:", error);
    }

    // Načtení historie objednávek
    const historyContainer = document.getElementById('orders-history-container');
    if (historyContainer) {
        try {
            const resOrders = await fetch(`get_orders.php?user_id=${userId}`);
            
            if (!resOrders.ok) throw new Error("Chyba spojení se serverem.");

            const orders = await resOrders.json();
            
            if (!orders || !Array.isArray(orders) || orders.length === 0) {
                historyContainer.innerHTML = '<p>Dosud nebyla žádná objednávka.</p>';
            } else {
                let html = '';
                orders.reverse().forEach((order, index) => {
                    const uniqueId = order.id || index; // Unikátní ID pro toggle funkce
                    
                    // Zpracování produktů (předpokládá, že PHP vrací pole order.products)
                    let productsHtml = '';
                    if (order.products && Array.isArray(order.products) && order.products.length > 0) {
                        productsHtml = order.products.map(p => 
                            `<div style="display: flex; justify-content: space-between; border-bottom: 1px solid #eee; padding: 8px 0; font-size: 14px;">
                                <span>${p.quantity ? p.quantity + 'x ' : ''}${p.name || 'Neznámý produkt'}</span>
                                <span>${p.price ? p.price + ' Kč' : ''}</span>
                            </div>`
                        ).join('');
                    } else {
                        productsHtml = '<p style="font-size: 14px; color: #777; margin: 0;">Detaily produktů nejsou k dispozici.</p>';
                    }

                    html += `
                        <div style="border: 1px solid #ddd; padding: 15px; margin-bottom: 10px; border-radius: 8px; background: #fafafa;">
                            <h3 style="margin-top: 0; font-size: 16px; color: #333;">Objednávka ze dne: ${order.created_at || 'Neznámé datum'}</h3>
                            <p style="margin: 5px 0; font-size: 14px;"><strong>Celková cena:</strong> ${order.total || '0'} Kč</p>
                            <p style="margin: 5px 0; font-size: 14px;"><strong>Doručovací adresa:</strong> ${order.street}, ${order.city} ${order.zip}</p>
                            
                            <!-- Dropdown tlačítko pro zobrazení produktů -->
                            <div style="margin-top: 15px; border-top: 1px solid #eee; padding-top: 10px;">
                                <button type="button" onclick="toggleProducts('products-${uniqueId}', this)" style="background: none; border: none; color: #111; font-weight: 600; font-size: 14px; cursor: pointer; padding: 0; text-decoration: underline;">
                                    Zobrazit zakoupené produkty
                                </button>
                            </div>
                            
                            <!-- Skrytá sekce s produkty -->
                            <div id="products-${uniqueId}" style="display: none; margin-top: 15px; padding-top: 5px;">
                                ${productsHtml}
                            </div>
                        </div>
                    `;
                });
                historyContainer.innerHTML = html;
            }
        } catch (err) {
            console.error("Chyba při načítání objednávek:", err);
            historyContainer.innerHTML = '<p>Dosud nebyla žádná objednávka.</p>';
        }
    }
});

// Odeslání formuláře (zahrnuje profil i heslo v jednom kroku)
document.getElementById('checkoutForm').addEventListener('submit', async (e) => {
    e.preventDefault(); 
    
    const userId = localStorage.getItem('userId') || 'guest';
    
    // 1. ZPRACOVÁNÍ OSOBNÍCH ÚDAJŮ
    const profileData = {
        user_id: userId,
        name: document.getElementById('name').value,
        street: document.getElementById('street').value,
        zip: document.getElementById('zip').value,
        city: document.getElementById('city').value,
        phone: document.getElementById('phone').value,
        email: document.getElementById('email').value
    };

    try {
        const profileResponse = await fetch('profile.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(profileData)
        });
        const profileResult = await profileResponse.json();
        
        if (!profileResult.success) {
            alert("Chyba při ukládání údajů: " + profileResult.message);
            return; 
        }

        if (profileResult.new_email) {
            localStorage.setItem('userId', profileResult.new_email);
        }

        // 2. KONTROLA A ZPRACOVÁNÍ HESLA
        const oldPassword = document.getElementById('old-password').value;
        const newPassword = document.getElementById('new-password').value;

        // Pokud je sekce zobrazená a uživatel něco vepsal do hesel
        const isSectionVisible = document.getElementById('password-section').style.display === 'block';

        if (isSectionVisible && (oldPassword || newPassword)) {
            if (!oldPassword || !newPassword) {
                alert("Pro změnu hesla musíte vyplnit současné i nové heslo.");
                return;
            }

            const passResponse = await fetch('profile.php?action=change_password', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    user_id: localStorage.getItem('userId'),
                    old_password: oldPassword,
                    new_password: newPassword
                })
            });
            const passResult = await passResponse.json();
            
            if (passResult.success) {
                alert("Osobní údaje i heslo byly úspěšně změněny!");
                
                // Po úspěšné změně vyčistíme a skryjeme sekci s heslem
                document.getElementById('old-password').value = '';
                document.getElementById('new-password').value = '';
                
                const section = document.getElementById('password-section');
                const btn = document.getElementById('show-password-btn');
                section.style.display = 'none';
                btn.textContent = 'Změnit heslo';
                btn.style.color = '#111';
                btn.style.textDecoration = 'underline';

            } else {
                alert("Údaje uloženy, ale heslo se nepodařilo změnit: " + passResult.message);
            }
        } else {
            // Heslo nebylo vyplněno/změněno, zobrazíme úspěch jen pro data
            alert(profileResult.message);
        }

    } catch (error) {
        console.error("Chyba při odesílání:", error);
        alert("Nepodařilo se odeslat data na server.");
    }
});

// Funkce pro rozbalení/sbalení formuláře pro změnu hesla
function togglePasswordSection() {
    const section = document.getElementById('password-section');
    const btn = document.getElementById('show-password-btn');
    
    if (section.style.display === 'none' || section.style.display === '') {
        section.style.display = 'block';
        btn.textContent = 'Zrušit změnu hesla';
        btn.style.color = '#777';
        btn.style.textDecoration = 'none';
    } else {
        section.style.display = 'none';
        btn.textContent = 'Změnit heslo';
        btn.style.color = '#111';
        btn.style.textDecoration = 'underline';
        
        // Vymazání polí při skrytí (aby to nezůstalo vyplněné omylem)
        document.getElementById('old-password').value = '';
        document.getElementById('new-password').value = '';
    }
}

// Nová funkce pro rozbalení/sbalení seznamu produktů u objednávky
function toggleProducts(containerId, btnElement) {
    const container = document.getElementById(containerId);
    
    if (container.style.display === 'none' || container.style.display === '') {
        container.style.display = 'block';
        btnElement.textContent = 'Skrýt zakoupené produkty';
        btnElement.style.color = '#777';
        btnElement.style.textDecoration = 'none';
    } else {
        container.style.display = 'none';
        btnElement.textContent = 'Zobrazit zakoupené produkty';
        btnElement.style.color = '#111';
        btnElement.style.textDecoration = 'underline';
    }
}

// Funkce na přepínání viditelnosti hesla s prohozenými ikonami
function toggleVisibility(inputId, iconElement) {
    const input = document.getElementById(inputId);
    
    if (input.type === 'password') {
        input.type = 'text'; // Odkryje heslo
        // Změní ikonu na NORMÁLNÍ OKO
        iconElement.innerHTML = `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#666" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg>`;
    } else {
        input.type = 'password'; // Skryje heslo
        // Změní ikonu na PŘEŠKRTNUTÉ OKO
        iconElement.innerHTML = `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#666" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1="1" y1="1" x2="23" y2="23"></line></svg>`;
    }
}