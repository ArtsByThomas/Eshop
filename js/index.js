let cart = JSON.parse(localStorage.getItem('cart')) || [];

function updateCartUI() {
    const shopDropdown = document.getElementById('user-shop-dropdown');
    const badge = document.getElementById('cart-badge');
    
    const totalItems = cart.reduce((sum, item) => sum + item.quantity, 0);
    
    if (totalItems > 0) {
        badge.textContent = totalItems;
        badge.style.display = 'flex';
    } else {
        badge.style.display = 'none';
    }

    if (cart.length === 0) {
        shopDropdown.innerHTML = '<div style="padding: 2rem; text-align: center; color: #777;">Váš košík je prázdný</div>';
        return;
    }
    
    let cartHTML = '<div class="cart-items-scroll">';
    cart.forEach((item, index) => {
        cartHTML += `
            <div style="display:flex; align-items:center; margin-bottom: 1.5rem;">
                <img src="${item.image}" alt="${item.name}" style="width: 65px; height: 65px; object-fit: cover; border-radius: 8px; flex-shrink: 0;">
                <div style="display:flex; flex-direction:column; margin-left: 15px; flex: 1; min-width: 0;">
                    <strong style="color: #111; font-size: 0.95rem; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">${item.name}</strong>
                    <span style="color: #777; font-size: 0.85rem; margin-top: 4px;">${item.quantity}x ${item.price}</span>
                </div>
                <button onclick="removeFromCart(${index})" style="cursor:pointer; background:none; border:none; color:#ccc; font-size: 1.2rem; transition: color 0.2s; padding: 0 5px;" onmouseover="this.style.color='red'" onmouseout="this.style.color='#ccc'">✕</button>
            </div>
        `;
    });
    cartHTML += '</div>';
    cartHTML += `
        <div style="padding: 1.5rem; border-top: 1px solid #eaeaea; background-color: var(--card-bg);">
            <a href="checkout.html" style="background: #111; color: white; padding: 1rem; border-radius: 8px; text-align: center; display: block; font-weight: 600; text-decoration: none;">Přejít k platbě</a>
        </div>
    `;
    shopDropdown.innerHTML = cartHTML;
}

window.addToCart = function(id, name, price, image, event) {
    if(event) event.stopPropagation(); 
    const existingProduct = cart.find(p => p.id === id && p.image === image); 
    if (existingProduct) {
        existingProduct.quantity++;
    } else {
        cart.push({ id, name, price, image, quantity: 1 });
    }
    localStorage.setItem('cart', JSON.stringify(cart));
    updateCartUI();
    
    const dropdown = document.getElementById('user-shop-dropdown');
    dropdown.style.display = 'flex';
    setTimeout(() => { dropdown.style.display = 'none'; }, 2000);
}

window.removeFromCart = function(index) {
    cart.splice(index, 1);
    localStorage.setItem('cart', JSON.stringify(cart));
    updateCartUI();
}

let currentQuery = '';
let currentCategory = '';
let currentSubcategory = '';
let fetchedProducts = []; 

async function fetchAndRender() {
    try {
        let url = 'products.php?';
        if (currentQuery) url += `q=${encodeURIComponent(currentQuery)}&`;
        if (currentCategory) url += `category=${encodeURIComponent(currentCategory)}&`;
        if (currentSubcategory) url += `subcategory=${encodeURIComponent(currentSubcategory)}`;

        const response = await fetch(url);
        fetchedProducts = await response.json(); 

        renderProducts(fetchedProducts);
        renderFilters(fetchedProducts); 
    } catch (error) {
        console.error("Chyba při načítání:", error);
    }
}

function renderProducts(products) {
    const itemsContainer = document.getElementById("items");
    itemsContainer.innerHTML = ''; 

    if (products.length === 0) {
        itemsContainer.innerHTML = '<p style="grid-column: 1 / -1; font-size: 1.1rem; text-align: center; color: #777; margin-top: 3rem;">Žádné produkty neodpovídají vašemu výběru.</p>';
        return;
    }

    products.forEach(product => {
        itemsContainer.innerHTML += `
          <div class="item">
            <div class="item-clickable" onclick="openModal(${product.id})">
                <img src="${product.image}" alt="${product.name}">
                <div class="item-info">
                    <h3>${product.name}</h3>
                    <span class="price">${product.price}</span>
                </div>
            </div>
            <div style="padding: 0 1.5rem 1.5rem 1.5rem;">
                <button class="add-to-cart-btn" onclick="addToCart(${product.id}, '${product.name}', '${product.price}', '${product.image}', event)">
                    Přidat do košíku
                </button>
            </div>
        </div>
        `;
    });
}

function renderFilters(products) {
    const currentCats = {};
    products.forEach(p => {
        if (p.category) {
            if (!currentCats[p.category]) currentCats[p.category] = new Set();
            if (p.subcategory) currentCats[p.category].add(p.subcategory);
        }
    });

    const container = document.getElementById('filters-container');
    let html = `<div class="filter-row">`;
    const isAllActive = (!currentCategory && !currentQuery) ? 'active' : '';
    html += `<button class="filter-pill ${isAllActive}" onclick="applyFilter('', '')">Vše</button>`;
    for (const cat of Object.keys(currentCats)) {
        const isActive = cat === currentCategory ? 'active' : '';
        html += `<button class="filter-pill ${isActive}" onclick="applyFilter('${cat}', '')">${cat}</button>`;
    }
    html += `</div>`;

    if (currentCategory && currentCats[currentCategory] && currentCats[currentCategory].size > 0) {
        html += `<div class="filter-row">`;
        const isAllSubActive = !currentSubcategory ? 'active' : '';
        html += `<button class="filter-pill ${isAllSubActive}" onclick="applyFilter('${currentCategory}', '')">Vše z ${currentCategory}</button>`;
        for (const sub of currentCats[currentCategory]) {
            const isActive = sub === currentSubcategory ? 'active' : '';
            html += `<button class="filter-pill ${isActive}" onclick="applyFilter('${currentCategory}', '${sub}')">${sub}</button>`;
        }
        html += `</div>`;
    }
    container.innerHTML = html;
}

window.applyFilter = function(category, subcategory) {
    currentCategory = category;
    currentSubcategory = subcategory;
    if (!category && !subcategory) {
        currentQuery = '';
        document.getElementById('search-input').value = '';
    }
    fetchAndRender();
}

document.getElementById('search-input').addEventListener('input', (e) => {
    currentQuery = e.target.value.trim();
    currentCategory = '';
    currentSubcategory = '';
    fetchAndRender();
});

fetchAndRender();
updateCartUI();

const modal = document.getElementById('product-modal');
let currentImageSet = [];
let currentImageIndex = 0;

window.openModal = function(productId) {
    const product = fetchedProducts.find(p => p.id === productId);
    if(!product) return;

    document.getElementById('modal-title').textContent = product.name;
    document.getElementById('modal-price').textContent = product.price;
    document.getElementById('modal-desc').textContent = product.description || "Tento produkt zatím nemá podrobný popis.";
    
    const colorsContainer = document.getElementById('modal-colors');
    colorsContainer.innerHTML = '';

    if (product.colors && product.colors.length > 0) {
        product.colors.forEach((color, index) => {
            const isSelected = index === 0 ? 'selected' : ''; 
            
            const imagesArray = color.images || (color.img ? [color.img] : [product.image]);
            const imagesJson = JSON.stringify(imagesArray).replace(/'/g, "&apos;");

            colorsContainer.innerHTML += `
                <div class="color-swatch ${isSelected}" 
                     style="background-color: ${color.hex};" 
                     title="${color.name}"
                     data-images='${imagesJson}'
                     onclick="changeModalColor(this)">
                </div>
            `;
            
            if (index === 0) {
                currentImageSet = imagesArray;
                currentImageIndex = 0;
                updateGalleryUI();
            }
        });
    } else {
        currentImageSet = [product.image];
        currentImageIndex = 0;
        updateGalleryUI();
    }

    const addBtn = document.getElementById('modal-add-btn');
    addBtn.onclick = function() {
        addToCart(product.id, product.name, product.price, currentImageSet[0], null);
        closeModal();
    };

    modal.classList.add('show');
}

window.changeModalColor = function(element) {
    document.querySelectorAll('.color-swatch').forEach(s => s.classList.remove('selected'));
    element.classList.add('selected');
    
    currentImageSet = JSON.parse(element.getAttribute('data-images'));
    currentImageIndex = 0; 
    updateGalleryUI();
}

function updateGalleryUI() {
    document.getElementById('modal-img').src = currentImageSet[currentImageIndex];
    const dotsContainer = document.getElementById('gallery-dots');
    
    if (currentImageSet.length > 1) {
        dotsContainer.innerHTML = currentImageSet.map((_, i) => 
            `<div class="gallery-dot ${i === currentImageIndex ? 'active' : ''}" onclick="goToImage(${i}, event)"></div>`
        ).join('');
    } else {
        dotsContainer.innerHTML = '';
    }
}

window.goToImage = function(index, e) {
    if(e) e.stopPropagation();
    currentImageIndex = index;
    updateGalleryUI();
}

window.closeModal = function() {
    modal.classList.remove('show');
}
modal.addEventListener('click', function(e) {
    if(e.target === modal) closeModal();
});

// --- DROPDOWN LOGIKA A SPRÁVA ---
const profileBtn = document.getElementById('user-profile');
const userDropdown = document.getElementById('user-profile-dropdown');
const shopBtn = document.getElementById('user-shop');
const shopDropdown = document.getElementById('user-shop-dropdown');
const activeBtn = document.getElementById('active-orders');
const activeDropdown = document.getElementById('active-orders-dropdown');

function closeAllDropdowns() {
    if(userDropdown) userDropdown.style.display = 'none';
    if(shopDropdown) shopDropdown.style.display = 'none';
    if(activeDropdown) activeDropdown.style.display = 'none';
}

shopBtn.addEventListener('click', function(event) {
    event.stopPropagation(); 
    const isAlreadyOpen = shopDropdown.style.display === 'flex' || shopDropdown.style.display === 'block';
    closeAllDropdowns(); 
    if (!isAlreadyOpen) shopDropdown.style.display = 'flex';
});

profileBtn.addEventListener('click', function(event) {
    event.stopPropagation(); 
    const isLoggedIn = localStorage.getItem('isLoggedIn');
    
    if (isLoggedIn === 'true') {
        const isAlreadyOpen = userDropdown.style.display === 'flex' || userDropdown.style.display === 'block';
        closeAllDropdowns(); 
        if (!isAlreadyOpen) userDropdown.style.display = 'flex';
    } else {
        window.location.href = 'login.html';
    }
});

if (activeBtn) {
    activeBtn.addEventListener('click', function(event) {
        event.stopPropagation();
        const isAlreadyOpen = activeDropdown.style.display === 'flex' || activeDropdown.style.display === 'block';
        closeAllDropdowns();
        if (!isAlreadyOpen) activeDropdown.style.display = 'block';
    });
}

document.addEventListener('click', closeAllDropdowns);
if(shopDropdown) shopDropdown.addEventListener('click', e => e.stopPropagation());
if(userDropdown) userDropdown.addEventListener('click', e => e.stopPropagation());
if(activeDropdown) activeDropdown.addEventListener('click', e => e.stopPropagation());

function updateProfileMenu() {
    const profileText = document.getElementById('profile-text');
    const profileIcon = document.getElementById('profile-icon');
    if (!userDropdown) return;

    userDropdown.style.whiteSpace = 'nowrap';
    userDropdown.style.minWidth = '220px'; 

    const isLoggedIn = localStorage.getItem('isLoggedIn');

    if (isLoggedIn === 'true') {
        if (profileText) profileText.style.display = 'none';
        if (profileIcon) profileIcon.style.display = 'block';
        
        userDropdown.innerHTML = `
            <a href="ucet.html" style="text-decoration: none; color: inherit; display: block;">
                <li style="padding: 12px 15px; border-bottom: 1px solid #eee;">Můj účet a Historie</li>
            </a>
            <li id="logout-btn" style="cursor: pointer; color: red; padding: 12px 15px;">Odhlásit se</li>
        `;
        
        document.getElementById('logout-btn').addEventListener('click', (e) => {
            e.stopPropagation();
            localStorage.removeItem('isLoggedIn');
            localStorage.removeItem('userId');
            window.location.reload(); 
        });
    } else {
        if (profileIcon) profileIcon.style.display = 'none';
        if (profileText) {
            profileText.style.display = 'block';
            profileText.textContent = "Přihlásit se";
        }
        userDropdown.innerHTML = ''; 
    }
}
updateProfileMenu();

// --- NAČÍTÁNÍ AKTIVNÍCH OBJEDNÁVEK (Dropdown data) ---
async function fetchActiveOrders() {
    const activeBtn = document.getElementById('active-orders');
    const activeContent = document.getElementById('active-orders-content');
    const activeBadge = document.getElementById('active-orders-badge');
    const userId = localStorage.getItem('userId');

    // Pokud uživatel není přihlášen, skryjeme úplně tlačítko balíčku
    if (!userId || userId === 'guest') {
        if(activeBtn) activeBtn.style.display = 'none';
        return;
    }

    try {
        const response = await fetch(`get_orders.php?user_id=${userId}`);
        if (!response.ok) throw new Error("Chyba serveru");
        
        const orders = await response.json();
        
        // Vyfiltrování aktivních objednávek
        const activeOrders = orders.filter(o => o.status !== 'Doručeno' && o.status !== 'Zrušeno');

        if (activeOrders.length === 0) {
            // Pokud není co odesílat, schováme celou položku v menu!
            if(activeBtn) activeBtn.style.display = 'none';
        } else {
            // Máme objednávky, zobrazíme tlačítko
            if(activeBtn) activeBtn.style.display = 'flex';
            
            if(activeBadge) {
                activeBadge.textContent = activeOrders.length;
                activeBadge.style.display = 'flex';
            }

           let html = '';
            activeOrders.reverse().forEach(order => {
                
                // 1. Zjistíme nakoupené položky bez cen
                let itemsHtml = '';
                if (order.cart_items && order.cart_items.length > 0) {
                    itemsHtml = '<div class="order-items">';
                    order.cart_items.forEach(item => {
                        itemsHtml += `
                            <div style="font-size: 13px; color: #555; margin-bottom: 4px;">
                                <span style="white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">${item.quantity}x ${item.name}</span>
                            </div>
                        `;
                    });
                    itemsHtml += '</div>';
                }

                // 2. Čistý vizuál s kartami a odhadem doručení (bez ceny a data vytvoření)
                html += `
                    <div class="order-card">
                        <div class="order-delivery-badge">
                            Očekávané doručení: do 48 hodin
                        </div>
                        <div style="font-size: 13px; color: #333; line-height: 1.4; font-weight: 500;">
                            ${order.street}, ${order.city}
                        </div>
                        ${itemsHtml} 
                    </div>
                `;
            });
            
            if(activeContent) {
                activeContent.innerHTML = html;
            }
        }
    } catch (error) {
        console.error("Chyba načítání objednávek:", error);
        // Při chybě raději tlačítko vůbec neukážeme, ať to uživatele nezmate
        if(activeBtn) activeBtn.style.display = 'none';
    }
}
// Spuštění načítání dat ihned při načtení stránky
fetchActiveOrders();