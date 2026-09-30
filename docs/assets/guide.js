/* Documentation locale : recherche sans réseau, navigation et impression. */
(() => {
  const t = JSON.parse(document.querySelector('#guide-strings').textContent);
  const normalize = value => value.normalize('NFD').replace(/[\u0300-\u036f]/g, '').replace(/[-‑–—]/g, '').toLowerCase();
  const faq = document.querySelector('#faq');
  if (faq) {
    const items = [...faq.querySelectorAll('.faq-item')];
    const tools = document.createElement('div');
    tools.className = 'faq-tools';
    tools.innerHTML = '<div class="faq-search"><label for="faq-search"></label><input id="faq-search" type="search" autocomplete="off"></div><button id="faq-toggle" type="button"></button>';
    tools.querySelector('label').textContent = t.searchLabel;
    tools.querySelector('input').placeholder = t.searchPlaceholder;
    faq.insertBefore(tools, items[0]);
    const count = document.createElement('p');
    count.className = 'faq-count';
    count.setAttribute('role', 'status');
    count.setAttribute('aria-live', 'polite');
    tools.after(count);
    const empty = document.createElement('p');
    empty.className = 'no-results';
    empty.textContent = t.empty;
    empty.hidden = true;
    faq.append(empty);
    const input = document.querySelector('#faq-search');
    const toggle = document.querySelector('#faq-toggle');
    const syncToggle = () => {
      const visible = items.filter(item => !item.hidden);
      toggle.textContent = visible.length && visible.every(item => item.open) ? t.collapse : t.expand;
      toggle.disabled = visible.length === 0;
    };
    let beforeSearch = null;
    const filter = () => {
      const aliases = { reset: document.documentElement.lang === 'en' ? 'reset' : 'zero', reinitialisation: 'zero', etalonnage: 'calibration' };
      const terms = normalize(input.value).trim().split(/\s+/).filter(Boolean).map(term => aliases[term] || term);
      if (terms.length && !beforeSearch) beforeSearch = items.map(item => item.open);
      items.forEach((item, i) => {
        item.hidden = !terms.every(term => normalize(item.textContent).includes(term));
        if (terms.length) item.open = !item.hidden;
        else if (beforeSearch) item.open = beforeSearch[i];
      });
      if (!terms.length) beforeSearch = null;
      const n = items.filter(item => !item.hidden).length;
      count.textContent = (terms.length ? (n === 1 ? t.resultOne : t.resultMany) : t.count).replace('{n}', n).replace('{total}', items.length);
      empty.hidden = n > 0;
      syncToggle();
    };
    input.addEventListener('input', filter);
    toggle.addEventListener('click', () => {
      const visible = items.filter(item => !item.hidden);
      const open = !visible.every(item => item.open);
      visible.forEach(item => { item.open = open; });
      syncToggle();
    });
    items.forEach(item => item.addEventListener('toggle', syncToggle));
    filter();
  }
  document.querySelectorAll('[data-guide-lang]').forEach(link => link.addEventListener('click', () => {
    const destination = new URL(link.href);
    destination.hash = location.hash;
    link.href = destination.href;
  }));
  document.querySelectorAll('.mobile-nav a').forEach(link => link.addEventListener('click', () => { document.querySelector('.mobile-nav').open = false; }));
  const links = [...document.querySelectorAll('.sidebar nav a[href^="#"]')];
  const sections = [...document.querySelectorAll('main section[id]')];
  const progress = document.querySelector('.reading-progress');
  let scrollQueued = false;
  function updatePosition() {
    let current = sections[0]?.id;
    for (const section of sections) if (section.getBoundingClientRect().top <= 160) current = section.id;
    links.forEach(link => {
      if (link.hash === `#${current}`) link.setAttribute('aria-current', 'location');
      else link.removeAttribute('aria-current');
    });
    const height = document.documentElement.scrollHeight - innerHeight;
    if (progress) progress.style.width = `${height > 0 ? Math.min(100, Math.max(0, scrollY / height * 100)) : 0}%`;
    scrollQueued = false;
  }
  addEventListener('scroll', () => { if (!scrollQueued) { scrollQueued = true; requestAnimationFrame(updatePosition); } }, { passive: true });
  addEventListener('resize', updatePosition);
  updatePosition();
  let printState = [];
  addEventListener('beforeprint', () => {
    printState = [...document.querySelectorAll('main details')].map(item => ({ item, open: item.open, hidden: item.hidden }));
    printState.forEach(({ item }) => { item.open = true; item.hidden = false; });
  });
  addEventListener('afterprint', () => printState.forEach(({ item, open, hidden }) => { item.open = open; item.hidden = hidden; }));
  document.querySelector('.print-button')?.addEventListener('click', () => window.print());
})();
