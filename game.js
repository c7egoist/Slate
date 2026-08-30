(() => {
  const canvas = document.getElementById('game');
  const ctx = canvas.getContext('2d');
  const $ = (id) => document.getElementById(id);
  const briefing = $('briefing');
  const endScreen = $('endScreen');
  const deployButton = $('deployButton');
  const restartButton = $('restartButton');
  const targetCard = $('targetCard');
  const targetName = $('targetName');
  const targetRange = $('targetRange');
  const healthText = $('healthText');
  const energyText = $('energyText');
  const healthBar = $('healthBar');
  const energyBar = $('energyBar');
  const speedReadout = $('speedReadout');
  const headingReadout = $('headingReadout');
  const objectiveCount = $('objectiveCount');
  const objectiveText = $('objectiveText');
  const radarContacts = $('radarContacts');
  const damageVignette = $('damageVignette');
  const hitMarker = $('hitMarker');
  const toast = $('toast');

  let W = 0, H = 0, dpr = 1, horizon = 0, focal = 0;
  let last = performance.now();
  let started = false, ended = false;
  let startTime = 0, elapsed = 0, flashTimer = 0, toastTimer = 0;
  let keys = {};
  let pointerLocked = false;
  let terrainTick = 0;
  let player, enemies, sparks, shockwaves, enemyBolts;

  const TAU = Math.PI * 2;
  const clamp = (n, min, max) => Math.max(min, Math.min(max, n));
  const lerp = (a, b, n) => a + (b - a) * n;
  const hash = (x, z) => {
    const v = Math.sin(x * 127.1 + z * 311.7) * 43758.5453123;
    return v - Math.floor(v);
  };
  const heightAt = (x, z) => {
    const broad = Math.sin(x * .021 + z * .011) * 2.9 + Math.cos(z * .028 - x * .009) * 2.1;
    const crags = Math.sin(x * .115) * 1.1 + Math.cos(z * .09 + x * .04) * .8;
    return broad + crags;
  };
  const formatTime = (secs) => `${String(Math.floor(secs / 60)).padStart(2, '0')}:${String(Math.floor(secs % 60)).padStart(2, '0')}`;

  function resize() {
    dpr = Math.min(window.devicePixelRatio || 1, 2);
    W = window.innerWidth; H = window.innerHeight;
    canvas.width = Math.floor(W * dpr); canvas.height = Math.floor(H * dpr);
    canvas.style.width = `${W}px`; canvas.style.height = `${H}px`;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    horizon = H * .445;
    focal = W * .72;
  }
  window.addEventListener('resize', resize);
  resize();

  function resetGame() {
    player = { x: 0, z: 0, yaw: .18, health: 100, energy: 100, speed: 0, firing: 0, damage: 0 };
    const locations = [
      [-30, 45], [24, 62], [-68, 95], [76, 122], [15, 138], [-117, 142], [108, 188], [-38, 205]
    ];
    enemies = locations.map((point, i) => ({
      id: i, x: point[0], z: point[1], baseX: point[0], baseZ: point[1], phase: i * 1.43,
      hp: i > 4 ? 3 : 2, maxHp: i > 4 ? 3 : 2, alive: true, hit: 0, cooldown: 2.5 + i * .24,
      type: i === 5 || i === 7 ? 'REVENANT' : 'WARDEN'
    }));
    sparks = []; shockwaves = []; enemyBolts = [];
    elapsed = 0; flashTimer = 0; terrainTick = 0;
    updateHud();
  }

  function camPoint(x, y, z) {
    const dx = x - player.x, dz = z - player.z;
    const sy = Math.sin(player.yaw), cy = Math.cos(player.yaw);
    const forward = dx * sy + dz * cy;
    const side = dx * cy - dz * sy;
    if (forward <= .2) return { forward, side, x: -9999, y: -9999 };
    return {
      forward, side,
      x: W * .5 + side / forward * focal,
      y: horizon + (4.25 - y) / forward * focal
    };
  }

  function sky() {
    const g = ctx.createLinearGradient(0, 0, 0, H);
    g.addColorStop(0, '#092d3b');
    g.addColorStop(.42, '#366d70');
    g.addColorStop(.49, '#d28d62');
    g.addColorStop(.67, '#c66e4d');
    ctx.fillStyle = g; ctx.fillRect(0, 0, W, H);

    const sx = W * .72, sy = horizon * .49;
    const sun = ctx.createRadialGradient(sx, sy, 1, sx, sy, W * .17);
    sun.addColorStop(0, 'rgba(255,236,168,.97)');
    sun.addColorStop(.14, 'rgba(255,203,118,.75)');
    sun.addColorStop(1, 'rgba(255,178,92,0)');
    ctx.fillStyle = sun; ctx.fillRect(sx - W*.2, sy-W*.2, W*.4, W*.4);
    ctx.fillStyle = '#ffe1a1'; ctx.beginPath(); ctx.arc(sx, sy, Math.min(40, W*.027), 0, TAU); ctx.fill();

    // Remote faceted mesas move imperceptibly with heading, making the basin feel broad.
    const offset = (player ? player.yaw * 85 + player.x * .03 : .3);
    drawMountainBand(horizon + 19, '#214c52', 55, 23, offset, .9);
    drawMountainBand(horizon + 30, '#173d45', 73, 35, offset + 19, 1.5);
    drawMountainBand(horizon + 40, '#102f38', 88, 42, offset + 41, 2.2);
  }

  function drawMountainBand(base, color, height, step, offset, jag) {
    ctx.fillStyle = color;
    ctx.beginPath(); ctx.moveTo(-step, H);
    for (let x = -step; x <= W + step * 2; x += step) {
      const n = Math.sin((x + offset) * .015 * jag) * .45 + Math.sin((x - offset) * .041) * .24 + hash(Math.floor((x + offset) / step), Math.floor(jag * 8)) * .42;
      const y = base - height * (n + .2);
      ctx.lineTo(x, y);
    }
    ctx.lineTo(W + step, H); ctx.closePath(); ctx.fill();
  }

  function terrain() {
    const tile = 13;
    const cx = Math.floor(player.x / tile), cz = Math.floor(player.z / tile);
    const faces = [];
    const radius = W < 720 ? 10 : 13;
    for (let gz = cz - radius; gz <= cz + radius; gz++) {
      for (let gx = cx - radius; gx <= cx + radius; gx++) {
        const x0 = gx * tile, z0 = gz * tile, x1 = x0 + tile, z1 = z0 + tile;
        const a = { x:x0, z:z0, y:heightAt(x0,z0) }, b = { x:x1,z:z0,y:heightAt(x1,z0) };
        const c = { x:x1,z:z1,y:heightAt(x1,z1) }, d = { x:x0,z:z1,y:heightAt(x0,z1) };
        const flip = hash(gx, gz) > .5;
        const triangles = flip ? [[a,b,d],[b,c,d]] : [[a,b,c],[a,c,d]];
        for (const tri of triangles) {
          const p = tri.map(v => camPoint(v.x,v.y,v.z));
          if (p.every(q => q.forward > 1.5)) {
            const distance = (p[0].forward + p[1].forward + p[2].forward) / 3;
            if (distance < 265 && !(p.every(q => q.y < -50) || p.every(q => q.y > H + 80))) faces.push({ p, tri, distance, gx, gz });
          }
        }
      }
    }
    faces.sort((a,b) => b.distance - a.distance);
    for (const face of faces) {
      const slope = (face.tri[0].y + face.tri[1].y + face.tri[2].y) / 3;
      const noise = hash(face.gx * 2 + (face.distance % 3), face.gz) - .5;
      const depth = clamp(face.distance / 280, 0, 1);
      const r = Math.round(lerp(128 + slope * 5 + noise * 10, 32, depth));
      const g = Math.round(lerp(91 + slope * 6 + noise * 11, 60, depth));
      const b = Math.round(lerp(54 + slope * 3, 58, depth));
      ctx.fillStyle = `rgb(${clamp(r,20,175)},${clamp(g,35,126)},${clamp(b,35,84)})`;
      ctx.beginPath(); ctx.moveTo(face.p[0].x, face.p[0].y); ctx.lineTo(face.p[1].x, face.p[1].y); ctx.lineTo(face.p[2].x, face.p[2].y); ctx.closePath(); ctx.fill();
      if (face.distance < 120 && hash(face.gx,face.gz) > .72) {
        ctx.strokeStyle = 'rgba(255,214,145,.08)'; ctx.lineWidth = .5;
        ctx.stroke();
      }
    }
  }

  function worldObjects() {
    // small low-poly basalt shards to offer speed and scale cues
    const tile = 25, radius = 9;
    const cx = Math.floor(player.x/tile), cz = Math.floor(player.z/tile);
    const rocks = [];
    for (let z=cz-radius; z<=cz+radius; z++) for (let x=cx-radius; x<=cx+radius; x++) {
      const h = hash(x,z); if (h < .74) continue;
      const rx=x*tile+3+hash(x+9,z)*18, rz=z*tile+3+hash(x,z+11)*18;
      const p=camPoint(rx,heightAt(rx,rz),rz);
      if (p.forward>5 && p.forward<170 && p.x>-30 && p.x<W+30) rocks.push({p, size:(.55+h*1.4)*focal/p.forward, h});
    }
    rocks.sort((a,b)=>b.p.forward-a.p.forward);
    for (const rock of rocks) {
      const {p,size,h}=rock; if(size<1)continue;
      ctx.fillStyle='rgba(30,40,37,.46)';ctx.beginPath();ctx.ellipse(p.x,p.y+2*size,size*1.4,size*.27,0,0,TAU);ctx.fill();
      ctx.fillStyle=h>.88?'#566150':'#3d4e42';ctx.beginPath();ctx.moveTo(p.x-size,p.y);ctx.lineTo(p.x-size*.35,p.y-size*1.6);ctx.lineTo(p.x+size*.82,p.y-size*.5);ctx.lineTo(p.x+size*.65,p.y);ctx.closePath();ctx.fill();
      ctx.fillStyle='rgba(206,181,117,.22)';ctx.beginPath();ctx.moveTo(p.x-size*.35,p.y-size*1.6);ctx.lineTo(p.x+size*.82,p.y-size*.5);ctx.lineTo(p.x+size*.2,p.y-size*.38);ctx.closePath();ctx.fill();
    }
  }

  function drawEnemy(enemy) {
    const ground = heightAt(enemy.x, enemy.z);
    const p = camPoint(enemy.x, ground, enemy.z);
    if (p.forward < 3 || p.forward > 280 || p.x < -140 || p.x > W+140) return;
    const scale = focal / p.forward;
    const width = scale * (enemy.type === 'REVENANT' ? 4.7 : 4.05);
    const height = scale * 2.35;
    if (width < 1.2) return;
    const hit = enemy.hit > 0;
    ctx.save();
    ctx.translate(p.x,p.y);
    ctx.fillStyle='rgba(27,32,29,.42)';ctx.beginPath();ctx.ellipse(0,3,width*.82,height*.17,0,0,TAU);ctx.fill();
    // undercarriage
    ctx.fillStyle = '#182c2e';ctx.beginPath();ctx.moveTo(-width*.76,0);ctx.lineTo(width*.76,0);ctx.lineTo(width*.59,-height*.32);ctx.lineTo(-width*.59,-height*.32);ctx.closePath();ctx.fill();
    // low angular hull, with a bright hostile face
    ctx.fillStyle = hit ? '#e6b16e' : '#7d4d45';ctx.beginPath();ctx.moveTo(-width*.7,-height*.31);ctx.lineTo(width*.7,-height*.31);ctx.lineTo(width*.46,-height*.91);ctx.lineTo(-width*.46,-height*.91);ctx.closePath();ctx.fill();
    ctx.fillStyle = hit ? '#ffdf9a' : '#a45d4b';ctx.beginPath();ctx.moveTo(-width*.46,-height*.91);ctx.lineTo(width*.46,-height*.91);ctx.lineTo(width*.17,-height*1.1);ctx.lineTo(-width*.17,-height*1.1);ctx.closePath();ctx.fill();
    ctx.fillStyle='#213638';ctx.beginPath();ctx.moveTo(-width*.25,-height*.88);ctx.lineTo(width*.25,-height*.88);ctx.lineTo(width*.16,-height*1.27);ctx.lineTo(-width*.16,-height*1.27);ctx.closePath();ctx.fill();
    // cannon oriented generally at Sentinel, projection creates its dramatic spear profile
    ctx.strokeStyle = hit ? '#ffd28a' : '#293a39';ctx.lineWidth=Math.max(1,width*.12);ctx.beginPath();ctx.moveTo(0,-height*1.16);ctx.lineTo(0,-height*1.74);ctx.stroke();
    ctx.fillStyle='#ff7657';ctx.shadowBlur=width*.17;ctx.shadowColor='#ff563d';ctx.fillRect(-width*.1,-height*.73,width*.2,Math.max(1,height*.09));ctx.shadowBlur=0;
    const barW=width*.78;ctx.fillStyle='rgba(6,20,22,.75)';ctx.fillRect(-barW/2,-height*1.95,barW,Math.max(2,height*.08));ctx.fillStyle=hit?'#ffda87':'#ff7157';ctx.fillRect(-barW/2,-height*1.95,barW*(enemy.hp/enemy.maxHp),Math.max(2,height*.08));
    ctx.restore();
  }

  function renderBolts() {
    for (const bolt of enemyBolts) {
      const a = camPoint(bolt.x, heightAt(bolt.x,bolt.z)+1.7, bolt.z);
      const px = bolt.x - bolt.dx * 4, pz = bolt.z - bolt.dz * 4;
      const b = camPoint(px,heightAt(px,pz)+1.7,pz);
      if(a.forward>1 && b.forward>1) {
        ctx.strokeStyle='rgba(255,112,77,.85)';ctx.lineWidth=clamp(focal/a.forward*.09,1,4);ctx.shadowBlur=9;ctx.shadowColor='#ff573c';ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke();ctx.shadowBlur=0;
      }
    }
    for (const spark of sparks) {
      const p=camPoint(spark.x,spark.y,spark.z); if(p.forward<1)continue;
      ctx.fillStyle=spark.color; ctx.globalAlpha=clamp(spark.life*2.2,0,1);ctx.beginPath();ctx.arc(p.x,p.y,clamp(spark.size*focal/p.forward,1,8),0,TAU);ctx.fill();ctx.globalAlpha=1;
    }
    for (const wave of shockwaves) {
      const p=camPoint(wave.x,heightAt(wave.x,wave.z)+.4,wave.z);if(p.forward<1)continue;
      const s=focal/p.forward*(.6+(1-wave.life)*5);
      ctx.strokeStyle=`rgba(255,188,103,${wave.life*.65})`;ctx.lineWidth=2;ctx.beginPath();ctx.arc(p.x,p.y,s,0,TAU);ctx.stroke();
    }
  }

  function cockpit() {
    const grd=ctx.createLinearGradient(0,H*.7,0,H);grd.addColorStop(0,'rgba(4,13,17,0)');grd.addColorStop(1,'rgba(4,13,17,.78)');ctx.fillStyle=grd;ctx.fillRect(0,H*.72,W,H*.28);
    ctx.strokeStyle='rgba(142,225,210,.18)';ctx.lineWidth=1;
    ctx.beginPath();ctx.moveTo(0,H*.81);ctx.lineTo(W*.12,H);ctx.moveTo(W,H*.81);ctx.lineTo(W*.88,H);ctx.stroke();
    ctx.fillStyle='rgba(6,20,25,.45)';ctx.beginPath();ctx.moveTo(W*.39,H);ctx.lineTo(W*.435,H*.914);ctx.lineTo(W*.565,H*.914);ctx.lineTo(W*.61,H);ctx.closePath();ctx.fill();
  }

  function render() {
    sky();
    terrain();
    worldObjects();
    const sortedEnemies = enemies.filter(e=>e.alive).sort((a,b)=> {
      const da=(a.x-player.x)**2+(a.z-player.z)**2, db=(b.x-player.x)**2+(b.z-player.z)**2; return db-da;
    });
    sortedEnemies.forEach(drawEnemy);
    renderBolts();
    cockpit();
  }

  function getTarget() {
    let best=null, bestScore=Infinity;
    for (const e of enemies) {
      if (!e.alive) continue;
      const dx=e.x-player.x,dz=e.z-player.z;
      const f=dx*Math.sin(player.yaw)+dz*Math.cos(player.yaw), s=dx*Math.cos(player.yaw)-dz*Math.sin(player.yaw);
      if(f<5) continue;
      const score=Math.abs(s/f)*140 + f*.004;
      if(Math.abs(s/f)<.09 && score<bestScore) { best=e;bestScore=score; }
    }
    return best;
  }

  function fire() {
    if (!started || ended || player.firing > 0 || player.energy < 12) return;
    player.firing=.24; player.energy-=12;
    const target=getTarget();
    if(target) {
      target.hp--;target.hit=.16;
      flashTimer=.19; hitMarker.classList.add('show');
      setTimeout(()=>hitMarker.classList.remove('show'),120);
      shockwaves.push({x:target.x,z:target.z,life:1});
      for(let i=0;i<11;i++) sparks.push({x:target.x+(Math.random()-.5)*2,z:target.z+(Math.random()-.5)*2,y:heightAt(target.x,target.z)+1+Math.random()*2,life:.35+Math.random()*.45,size:.12+Math.random()*.18,color:Math.random()>.5?'#ffd284':'#ff7858',vx:(Math.random()-.5)*6,vz:(Math.random()-.5)*6,vy:Math.random()*4});
      if(target.hp<=0) destroy(target);
    } else {
      showToast('LANCE DISCHARGED — NO LOCK');
    }
  }

  function destroy(enemy) {
    enemy.alive=false;
    for(let i=0;i<30;i++) sparks.push({x:enemy.x+(Math.random()-.5)*4,z:enemy.z+(Math.random()-.5)*4,y:heightAt(enemy.x,enemy.z)+Math.random()*3,life:.5+Math.random()*.8,size:.1+Math.random()*.34,color:Math.random()>.35?'#ff815a':'#ffdc92',vx:(Math.random()-.5)*14,vz:(Math.random()-.5)*14,vy:Math.random()*10});
    shockwaves.push({x:enemy.x,z:enemy.z,life:1});
    const kills=enemies.filter(e=>!e.alive).length;
    objectiveCount.textContent=`${kills} / ${enemies.length}`;
    showToast(kills === enemies.length ? 'ALL HOSTILE ARMOR NEUTRALIZED' : `${enemy.type} DESTROYED  +100`);
    if(kills===enemies.length) setTimeout(win,900);
  }

  function enemyFire(enemy) {
    const dx=player.x-enemy.x,dz=player.z-enemy.z,dist=Math.hypot(dx,dz);
    enemyBolts.push({x:enemy.x,z:enemy.z,dx:dx/dist,dz:dz/dist,life:2.4,speed:75});
  }

  function damage(amount) {
    player.health=clamp(player.health-amount,0,100);player.damage=1;
    damageVignette.style.opacity = `${.18 + (100-player.health)/155}`;
    setTimeout(()=>damageVignette.style.opacity='0',160);
    if(player.health<=0) lose();
  }

  function update(dt) {
    if(!started || ended) return;
    elapsed=(performance.now()-startTime)/1000;
    terrainTick+=dt;
    let forwardInput=(keys.KeyW||keys.ArrowUp?1:0)-(keys.KeyS||keys.ArrowDown?1:0);
    let strafeInput=(keys.KeyD?1:0)-(keys.KeyA?1:0);
    if(keys.ArrowLeft)player.yaw-=dt*1.3;if(keys.ArrowRight)player.yaw+=dt*1.3;
    const desired=forwardInput*31;
    player.speed=lerp(player.speed,desired,Math.min(1,dt*4.2));
    const sy=Math.sin(player.yaw),cy=Math.cos(player.yaw);
    player.x+=sy*player.speed*dt+cy*strafeInput*17*dt;
    player.z+=cy*player.speed*dt-sy*strafeInput*17*dt;
    player.energy=clamp(player.energy+dt*16,0,100);
    player.firing=Math.max(0,player.firing-dt);player.damage=Math.max(0,player.damage-dt*2.7);
    for(const e of enemies) {
      if(!e.alive) continue;
      e.hit=Math.max(0,e.hit-dt*5);
      const dx=player.x-e.x,dz=player.z-e.z,dist=Math.hypot(dx,dz);
      if(dist>43) { e.x+=dx/dist*dt*(2.1+(e.id%2)*.35); e.z+=dz/dist*dt*(2.1+(e.id%2)*.35); }
      else { const cir=(elapsed+e.phase)*.55;e.x+=Math.cos(cir)*dt*3.1;e.z+=Math.sin(cir)*dt*3.1; }
      e.cooldown-=dt;
      if(e.cooldown<0 && dist<155) { enemyFire(e);e.cooldown=2.1+Math.random()*2.2; }
    }
    for(const bolt of enemyBolts) { bolt.x+=bolt.dx*bolt.speed*dt;bolt.z+=bolt.dz*bolt.speed*dt;bolt.life-=dt; if(Math.hypot(player.x-bolt.x,player.z-bolt.z)<3.1){bolt.life=0;damage(8+Math.random()*5);} }
    enemyBolts=enemyBolts.filter(b=>b.life>0);
    for(const s of sparks) { s.x+=s.vx*dt;s.z+=s.vz*dt;s.y+=s.vy*dt;s.vy-=12*dt;s.life-=dt; }
    sparks=sparks.filter(s=>s.life>0);
    for(const w of shockwaves)w.life-=dt;shockwaves=shockwaves.filter(w=>w.life>0);
    updateHud();
  }

  function updateHud() {
    if(!player) return;
    const kills=enemies ? enemies.filter(e=>!e.alive).length : 0;
    healthText.textContent=Math.round(player.health);energyText.textContent=Math.round(player.energy);
    healthBar.style.width=`${player.health}%`;energyBar.style.width=`${player.energy}%`;
    speedReadout.textContent=String(Math.round(Math.abs(player.speed)*4.8)).padStart(3,'0');
    const degrees=((player.yaw*180/Math.PI)%360+360)%360;const dirs=['N','NE','E','SE','S','SW','W','NW'];headingReadout.textContent=`${dirs[Math.round(degrees/45)%8]}  ${String(Math.round(degrees)).padStart(3,'0')}°`;
    objectiveCount.textContent=`${kills} / ${enemies ? enemies.length : 8}`;
    objectiveText.textContent=kills===8?'Approach cleared. Relay signal returning.':`${8-kills} hostile ${8-kills===1?'interceptor remains':'interceptors remain'} in the basin.`;
    const target=getTarget();
    targetCard.classList.toggle('active',!!target && started && !ended);
    if(target){targetName.textContent=target.type;targetRange.textContent=`${Math.round(Math.hypot(target.x-player.x,target.z-player.z))}M`;}
    radarContacts.innerHTML='';
    if(enemies) for(const e of enemies) if(e.alive) {
      const dx=e.x-player.x,dz=e.z-player.z;const dist=Math.hypot(dx,dz); if(dist>240)continue;
      const rel=Math.atan2(dx,dz)-player.yaw;const r=clamp(dist/240,0,1)*58;const dot=document.createElement('i');dot.className='contact';dot.style.left=`${70+Math.sin(rel)*r}px`;dot.style.top=`${70-Math.cos(rel)*r}px`;radarContacts.appendChild(dot);
    }
  }

  function showToast(message) { toast.textContent=message;toast.classList.add('show');toastTimer++;const id=toastTimer;setTimeout(()=>{if(id===toastTimer)toast.classList.remove('show');},1700); }
  function finish(title, kicker, copy) {
    ended=true;document.exitPointerLock?.();
    $('endTitle').innerHTML=title;$('endKicker').textContent=kicker;$('endCopy').textContent=copy;
    $('endKills').textContent=String(enemies.filter(e=>!e.alive).length).padStart(2,'0');$('endHealth').textContent=`${Math.round(player.health)}%`;$('endTime').textContent=formatTime(elapsed);
    endScreen.classList.remove('hidden');
  }
  function win(){finish('RELAY APPROACH<br /><em>SECURED.</em>','MISSION COMPLETE','The upland route is clear. Your signal carries beyond the ridge.');}
  function lose(){finish('SENTINEL<br /><em>OFFLINE.</em>','SIGNAL LOST','The basin has gone quiet. Rebuild the route and make another sweep.');}
  function start() { resetGame();started=true;ended=false;startTime=performance.now();briefing.classList.add('hidden');endScreen.classList.add('hidden');showToast('SENTINEL SYSTEMS ONLINE'); }

  deployButton.addEventListener('click',()=>{start();canvas.requestPointerLock?.();});
  restartButton.addEventListener('click',()=>{start();canvas.requestPointerLock?.();});
  window.addEventListener('keydown',e=>{keys[e.code]=true;if(e.code==='Space'){e.preventDefault();fire();}if(e.code==='Enter'&&!started)start();if(e.code==='KeyR'&&ended)start();});
  window.addEventListener('keyup',e=>keys[e.code]=false);
  canvas.addEventListener('click',()=>{if(started&&!ended){if(!pointerLocked)canvas.requestPointerLock?.();fire();}});
  document.addEventListener('pointerlockchange',()=>pointerLocked=document.pointerLockElement===canvas);
  document.addEventListener('mousemove',e=>{if(pointerLocked&&started&&!ended)player.yaw+=e.movementX*.0024;});

  function loop(now) {
    const dt=Math.min(.04,(now-last)/1000);last=now;
    if(!player) resetGame();
    update(dt);render();
    requestAnimationFrame(loop);
  }
  requestAnimationFrame(loop);
})();
