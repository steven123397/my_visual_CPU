const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

const revealItems = [...document.querySelectorAll('[data-reveal]')];
if (reduceMotion) {
  for (const item of revealItems) {
    item.classList.add('is-visible');
  }
} else {
  const revealObserver = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        if (entry.isIntersecting) {
          entry.target.classList.add('is-visible');
          revealObserver.unobserve(entry.target);
        }
      }
    },
    { rootMargin: '0px 0px -12% 0px', threshold: 0.18 },
  );
  for (const item of revealItems) {
    revealObserver.observe(item);
  }
}

const counters = [...document.querySelectorAll('[data-count]')];
function animateCount(element) {
  const target = Number(element.dataset.count ?? '0');
  if (!Number.isFinite(target)) {
    return;
  }
  if (reduceMotion || target === 0) {
    element.textContent = String(target);
    return;
  }
  const startTime = performance.now();
  const duration = 900;
  function tick(now) {
    const progress = Math.min((now - startTime) / duration, 1);
    const eased = 1 - Math.pow(1 - progress, 3);
    element.textContent = String(Math.round(target * eased));
    if (progress < 1) {
      requestAnimationFrame(tick);
    }
  }
  requestAnimationFrame(tick);
}

if (counters.length > 0) {
  const counterObserver = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        if (entry.isIntersecting) {
          animateCount(entry.target);
          counterObserver.unobserve(entry.target);
        }
      }
    },
    { threshold: 0.45 },
  );
  for (const counter of counters) {
    counterObserver.observe(counter);
  }
}

const parallaxItems = [...document.querySelectorAll('[data-parallax]')];
const motionTracks = [...document.querySelectorAll('[data-motion-track]')];

function updateMotion() {
  if (reduceMotion) {
    return;
  }
  const viewportHeight = window.innerHeight || 1;
  for (const item of parallaxItems) {
    const rect = item.getBoundingClientRect();
    const factor = Number(item.dataset.parallax ?? '0');
    const progress = (rect.top + rect.height / 2 - viewportHeight / 2) / viewportHeight;
    item.style.setProperty('--parallax-y', `${Math.round(progress * factor * -180)}px`);
    item.style.transform = `translateY(var(--parallax-y)) ${item.classList.contains('hero-screen--main') ? 'rotate(-2.5deg)' : item.classList.contains('hero-screen--side') ? 'rotate(3.5deg)' : ''}`;
  }
  for (const track of motionTracks) {
    const rect = track.getBoundingClientRect();
    const progress = Math.min(Math.max((viewportHeight - rect.top) / (viewportHeight + rect.height), 0), 1);
    track.style.setProperty('--track-progress', progress.toFixed(3));
  }
}

let ticking = false;
function requestMotionFrame() {
  if (ticking) {
    return;
  }
  ticking = true;
  requestAnimationFrame(() => {
    updateMotion();
    ticking = false;
  });
}

window.addEventListener('scroll', requestMotionFrame, { passive: true });
window.addEventListener('resize', requestMotionFrame);
requestMotionFrame();
