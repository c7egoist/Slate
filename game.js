(() => {
  'use strict';

  const canvas = document.getElementById('game');
  const gl = canvas.getContext('webgl', { antialias: true, alpha: false });
  const $ = (id) => document.getElementById(id);
  const briefing = $('briefing');
  const endScreen = $('endScreen');
  const deployButton = $('deployButton');
  const restartButton = $('restartButton');
  const muzzleFlash = $('muzzleFlash');
  const fireStreak = $('fireStreak');
  const healthText = $('healthText');
  const energyText = $('energyText');
  const healthBar = $('healthBar');
  const energyBar = $('energyBar');
  const speedReadout = $('speedReadout');
  const headingReadout = $('headingReadout');
  const targetRange = $('targetRange');
  const objectiveCount = $('objectiveCount');
  const objectiveText = $('objectiveText');
  const navHeading = $('navHeading');
  const playerMarker = $('playerMarker');
  const damageVignette = $('damageVignette');
  const toast = $('toast');

  if (!gl) {
    document.body.innerHTML = '<main style="height:100%;display:grid;place-items:center;background:#08161d;color:#e6efdc;font-family:monospace"><p>WebGL is required to run Sentinel Frontier.</p></main>';
    return;
  }

  const TAU = Math.PI * 2;
  const relay = { x: 84, z: 522 };
  const keys = Object.create(null);
  let started = false, ended = false, pointerLocked = false;
  let W = 1, H = 1, dpr = 1, startTime = 0, elapsed = 0, last = performance.now();
  let player, shots = [], particles = [], toastSerial = 0;
  let terrainBuffer, terrainVertices = 0, objectBuffer, objectVertices = 0, effectBuffer;
  let worldProgram, effectProgram, worldLoc, effectLoc;
  const projection = new Float32Array(16);
  const view = new Float32Array(16);

  const clamp = (number, min, max) => Math.max(min, Math.min(max, number));
  const lerp = (a, b, t) => a + (b - a) * t;
  const hash = (x, z) => {
    const value = Math.sin(x * 127.1 + z * 311.7) * 43758.5453123;
    return value - Math.floor(value);
  };
  // Broad slopes plus sharper ridges: a real heightfield shared by render, driving, and shots.
  const heightAt = (x, z) => {
    const basin = Math.sin(x * .015 + z * .010) * 8.2 + Math.cos(z * .017 - x * .006) * 6.4;
    const ridge = Math.sin(x * .062) * 2.8 + Math.cos(z * .047 + x * .032) * 2.3;
    const relayRise = 13 * Math.exp(-((x - relay.x) ** 2 + (z - relay.z) ** 2) / 26000);
    return basin + ridge + relayRise;
  };
  const normalize = (x, y, z) => {
    const length = Math.hypot(x, y, z) || 1;
    return [x / length, y / length, z / length];
  };
  const formatTime = (seconds) => `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(Math.floor(seconds % 60)).padStart(2, '0')}`;

  function shader(type, source) {
    const unit = gl.createShader(type);
    gl.shaderSource(unit, source);
    gl.compileShader(unit);
    if (!gl.getShaderParameter(unit, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(unit));
    return unit;
  }
  function program(vertexSource, fragmentSource) {
    const result = gl.createProgram();
    gl.attachShader(result, shader(gl.VERTEX_SHADER, vertexSource));
    gl.attachShader(result, shader(gl.FRAGMENT_SHADER, fragmentSource));
    gl.linkProgram(result);
    if (!gl.getProgramParameter(result, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(result));
    return result;
  }

  function setupGL() {
    worldProgram = program(`
      attribute vec3 aPosition; attribute vec3 aColor;
      uniform mat4 uProjection; uniform mat4 uView;
      varying vec3 vColor; varying float vDistance;
      void main() {
        vec4 eye = uView * vec4(aPosition, 1.0);
        vDistance = length(eye.xyz);
        vColor = aColor;
        gl_Position = uProjection * eye;
      }`, `
      precision mediump float;
      varying vec3 vColor; varying float vDistance;
      void main() {
        float fog = clamp(pow(vDistance / 420.0, 1.65), 0.0, 1.0);
        vec3 haze = vec3(0.60, 0.45, 0.34);
        gl_FragColor = vec4(mix(vColor, haze, fog), 1.0);
      }`);
    effectProgram = program(`
      attribute vec3 aPosition; attribute vec3 aColor; attribute float aSize;
      uniform mat4 uProjection; uniform mat4 uView;
      varying vec3 vColor;
      void main() {
        vec4 eye = uView * vec4(aPosition, 1.0);
        gl_Position = uProjection * eye;
        gl_PointSize = clamp(aSize * 720.0 / max(1.0, -eye.z), 2.0, 65.0);
        vColor = aColor;
      }`, `
      precision mediump float;
      varying vec3 vColor;
      void main() {
        vec2 point = gl_PointCoord - vec2(0.5);
        float radius = length(point);
        if (radius > 0.5) discard;
        gl_FragColor = vec4(vColor, 1.0 - radius * 1.5);
      }`);
    worldLoc = {
      position: gl.getAttribLocation(worldProgram, 'aPosition'), color: gl.getAttribLocation(worldProgram, 'aColor'),
      projection: gl.getUniformLocation(worldProgram, 'uProjection'), view: gl.getUniformLocation(worldProgram, 'uView')
    };
    effectLoc = {
      position: gl.getAttribLocation(effectProgram, 'aPosition'), color: gl.getAttribLocation(effectProgram, 'aColor'), size: gl.getAttribLocation(effectProgram, 'aSize'),
      projection: gl.getUniformLocation(effectProgram, 'uProjection'), view: gl.getUniformLocation(effectProgram, 'uView')
    };
    effectBuffer = gl.createBuffer();
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
    gl.clearColor(.04, .13, .17, 1);
  }

  function pushVertex(list, x, y, z, color) { list.push(x, y, z, color[0], color[1], color[2]); }
  function pushTriangle(list, a, b, c, color) { pushVertex(list, ...a, color); pushVertex(list, ...b, color); pushVertex(list, ...c, color); }
  function shadeFor(x, z, slope, tone) {
    const fleck = hash(Math.floor(x * .17), Math.floor(z * .17)) - .5;
    return [
      clamp(.39 + slope * .021 + tone + fleck * .075, .14, .68),
      clamp(.245 + slope * .019 + tone * .58 + fleck * .055, .13, .49),
      clamp(.125 + slope * .010 + tone * .22, .08, .28)
    ];
  }

  function createWorld() {
    const landscape = [];
    const step = 8, grid = 74;
    for (let gz = -grid; gz < grid; gz++) {
      for (let gx = -grid; gx < grid; gx++) {
        const x = gx * step, z = gz * step;
        const a = [x, heightAt(x, z), z], b = [x + step, heightAt(x + step, z), z];
        const c = [x + step, heightAt(x + step, z + step), z + step], d = [x, heightAt(x, z + step), z + step];
        const average = (a[1] + b[1] + c[1] + d[1]) * .25;
        const colorA = shadeFor(x, z, average, hash(gx, gz) * .08);
        const colorB = shadeFor(x + step, z + step, average, hash(gx + 4, gz + 7) * .07 - .01);
        if (hash(gx, gz) > .5) { pushTriangle(landscape, a, b, d, colorA); pushTriangle(landscape, b, c, d, colorB); }
        else { pushTriangle(landscape, a, b, c, colorA); pushTriangle(landscape, a, c, d, colorB); }
      }
    }
    terrainBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, terrainBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(landscape), gl.STATIC_DRAW);
    terrainVertices = landscape.length / 6;

    const objects = [];
    // Dense but deterministic rock clusters turn the heightfield into an actual traversable space.
    for (let gz = -29; gz < 63; gz++) for (let gx = -34; gx < 35; gx++) {
      const chance = hash(gx + 99, gz - 64);
      if (chance > .924 && Math.hypot(gx * 18 - relay.x, gz * 18 - relay.z) > 34) addRock(objects, gx * 18 + hash(gx, gz) * 13, gz * 18 + hash(gx + 3, gz + 4) * 13, .7 + chance * 2.4);
    }
    addRelay(objects);
    objectBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, objectBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(objects), gl.STATIC_DRAW);
    objectVertices = objects.length / 6;
  }

  function addRock(list, x, z, size) {
    const ground = heightAt(x, z), r = size;
    const h = r * (1.15 + hash(x, z));
    const a = [x-r,ground,z-r*.55], b=[x+r*.82,ground,z-r*.35], c=[x+r*.54,ground,z+r*.83], d=[x-r*.68,ground,z+r*.7], peak=[x-r*.1,ground+h,z+r*.08];
    pushTriangle(list,a,b,peak,[.22,.29,.25]);pushTriangle(list,b,c,peak,[.30,.33,.25]);pushTriangle(list,c,d,peak,[.17,.23,.21]);pushTriangle(list,d,a,peak,[.13,.20,.19]);
  }
  function addRelay(list) {
    const x = relay.x, z = relay.z, ground = heightAt(x, z), bottom = 1.7, top = 13;
    const a=[x-bottom,ground,z-bottom], b=[x+bottom,ground,z-bottom], c=[x+bottom,ground,z+bottom], d=[x-bottom,ground,z+bottom];
    const A=[x-.35,ground+top,z-.35], B=[x+.35,ground+top,z-.35], C=[x+.35,ground+top,z+.35], D=[x-.35,ground+top,z+.35];
    const dark=[.08,.22,.24], light=[.20,.57,.55], side=[.11,.37,.38];
    pushTriangle(list,a,b,B,dark);pushTriangle(list,a,B,A,dark);pushTriangle(list,b,c,C,side);pushTriangle(list,b,C,B,side);pushTriangle(list,c,d,D,light);pushTriangle(list,c,D,C,light);pushTriangle(list,d,a,A,[.06,.16,.18]);pushTriangle(list,d,A,D,[.06,.16,.18]);
    // antenna spire as narrow facets
    const p=[x,ground+top+7,z]; pushTriangle(list,A,B,p,light);pushTriangle(list,B,C,p,side);pushTriangle(list,C,D,p,dark);pushTriangle(list,D,A,p,[.10,.29,.30]);
  }

  function perspective(out, fieldOfView, aspect, near, far) {
    const f = 1 / Math.tan(fieldOfView / 2), nf = 1 / (near - far);
    out[0]=f/aspect;out[1]=0;out[2]=0;out[3]=0;out[4]=0;out[5]=f;out[6]=0;out[7]=0;out[8]=0;out[9]=0;out[10]=(far+near)*nf;out[11]=-1;out[12]=0;out[13]=0;out[14]=2*far*near*nf;out[15]=0;
  }
  function lookAt(out, eye, center, up) {
    let zx=eye[0]-center[0], zy=eye[1]-center[1], zz=eye[2]-center[2]; [zx,zy,zz]=normalize(zx,zy,zz);
    let xx=up[1]*zz-up[2]*zy, xy=up[2]*zx-up[0]*zz, xz=up[0]*zy-up[1]*zx; [xx,xy,xz]=normalize(xx,xy,xz);
    const yx=zy*xz-zz*xy, yy=zz*xx-zx*xz, yz=zx*xy-zy*xx;
    out[0]=xx;out[1]=yx;out[2]=zx;out[3]=0;out[4]=xy;out[5]=yy;out[6]=zy;out[7]=0;out[8]=xz;out[9]=yz;out[10]=zz;out[11]=0;out[12]=-(xx*eye[0]+xy*eye[1]+xz*eye[2]);out[13]=-(yx*eye[0]+yy*eye[1]+yz*eye[2]);out[14]=-(zx*eye[0]+zy*eye[1]+zz*eye[2]);out[15]=1;
  }

  function bindWorldBuffer(buffer) {
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.vertexAttribPointer(worldLoc.position, 3, gl.FLOAT, false, 24, 0); gl.enableVertexAttribArray(worldLoc.position);
    gl.vertexAttribPointer(worldLoc.color, 3, gl.FLOAT, false, 24, 12); gl.enableVertexAttribArray(worldLoc.color);
  }
  function drawEffects(camera) {
    const list = [];
    // Active lances are genuine projectiles in the same 3D world, not an HUD-only firing trick.
    for (const shot of shots) {
      list.push(shot.x,shot.y,shot.z, .55,.98,.85, .35);
      list.push(shot.x-shot.dx*2.4,shot.y-shot.dy*2.4,shot.z-shot.dz*2.4, .18,.68,.70, .20);
    }
    for (const particle of particles) list.push(particle.x,particle.y,particle.z,...particle.color,particle.size);
    list.push(relay.x, heightAt(relay.x,relay.z)+20.3, relay.z, .48,1,.86, 1.1);
    gl.useProgram(effectProgram);
    gl.uniformMatrix4fv(effectLoc.projection,false,projection);gl.uniformMatrix4fv(effectLoc.view,false,view);
    gl.bindBuffer(gl.ARRAY_BUFFER,effectBuffer);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(list),gl.DYNAMIC_DRAW);
    gl.vertexAttribPointer(effectLoc.position,3,gl.FLOAT,false,28,0);gl.enableVertexAttribArray(effectLoc.position);
    gl.vertexAttribPointer(effectLoc.color,3,gl.FLOAT,false,28,12);gl.enableVertexAttribArray(effectLoc.color);
    gl.vertexAttribPointer(effectLoc.size,1,gl.FLOAT,false,28,24);gl.enableVertexAttribArray(effectLoc.size);
    gl.enable(gl.BLEND);gl.blendFunc(gl.SRC_ALPHA,gl.ONE);gl.depthMask(false);gl.drawArrays(gl.POINTS,0,list.length/7);gl.depthMask(true);gl.disable(gl.BLEND);
  }

  function render() {
    const bob = started ? Math.sin(elapsed * 8) * Math.min(Math.abs(player.speed) / 33, 1) * .055 : 0;
    const ground = heightAt(player.x, player.z);
    const eye = [player.x, ground + 3.25 + bob, player.z];
    const direction = getDirection();
    lookAt(view, eye, [eye[0]+direction[0],eye[1]+direction[1],eye[2]+direction[2]], [0,1,0]);
    perspective(projection, 68 * Math.PI / 180, W / H, .1, 750);
    gl.viewport(0,0,canvas.width,canvas.height);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.useProgram(worldProgram);gl.uniformMatrix4fv(worldLoc.projection,false,projection);gl.uniformMatrix4fv(worldLoc.view,false,view);
    bindWorldBuffer(terrainBuffer);gl.drawArrays(gl.TRIANGLES,0,terrainVertices);
    bindWorldBuffer(objectBuffer);gl.drawArrays(gl.TRIANGLES,0,objectVertices);
    drawEffects(eye);
  }

  function getDirection() {
    const cosPitch = Math.cos(player.pitch);
    return [Math.sin(player.yaw) * cosPitch, Math.sin(player.pitch), Math.cos(player.yaw) * cosPitch];
  }

  function reset() {
    player = { x: 0, z: 0, yaw: .16, pitch: -.055, speed: 0, health: 100, energy: 100, firing: 0 };
    shots=[];particles=[];elapsed=0;updateHud();
  }
  function start() {
    reset(); started=true;ended=false;startTime=performance.now();briefing.classList.add('hidden');endScreen.classList.add('hidden');showToast('SENTINEL SYSTEMS ONLINE — LANCE CALIBRATED');
  }
  function showToast(message) { toast.textContent=message;toast.classList.add('show');const now=++toastSerial;setTimeout(()=>{if(now===toastSerial)toast.classList.remove('show');},1700); }

  function fire() {
    if (!started || ended || player.firing > 0 || player.energy < 9) return;
    player.firing=.16;player.energy-=9;
    const ground=heightAt(player.x,player.z), dir=getDirection();
    shots.push({x:player.x+dir[0]*2,y:ground+2.6+dir[1]*2,z:player.z+dir[2]*2,dx:dir[0],dy:dir[1],dz:dir[2],life:2.4});
    muzzleFlash.classList.remove('fire');fireStreak.classList.remove('fire');void muzzleFlash.offsetWidth;muzzleFlash.classList.add('fire');fireStreak.classList.add('fire');
  }
  function impact(shot) {
    for(let i=0;i<24;i++) {
      const angle=Math.random()*TAU, speed=4+Math.random()*18;
      particles.push({x:shot.x,y:shot.y+.08,z:shot.z,vx:Math.cos(angle)*speed,vy:3+Math.random()*13,vz:Math.sin(angle)*speed,life:.35+Math.random()*.65,size:.13+Math.random()*.42,color:Math.random()>.45?[1,.55,.22]:[.35,1,.86]});
    }
  }
  function finish() {
    ended=true;document.exitPointerLock?.();
    $('endTitle').innerHTML='RIDGEWAY LINK<br /><em>RESTORED.</em>';$('endKicker').textContent='MISSION COMPLETE';$('endCopy').textContent='The relay wakes above the basin. Your signal now reaches the expedition beyond the ridge.';
    $('endKills').textContent='00 M';$('endHealth').textContent=`${Math.round(player.health)}%`;$('endTime').textContent=formatTime(elapsed);endScreen.classList.remove('hidden');
  }
  function updateHud() {
    if(!player) return;
    const distance=Math.hypot(relay.x-player.x,relay.z-player.z);
    const degrees=((player.yaw*180/Math.PI)%360+360)%360, dirs=['N','NE','E','SE','S','SW','W','NW'];
    healthText.textContent=Math.round(player.health);energyText.textContent=Math.round(player.energy);healthBar.style.width=`${player.health}%`;energyBar.style.width=`${player.energy}%`;
    speedReadout.textContent=String(Math.round(Math.abs(player.speed)*5.3)).padStart(3,'0');headingReadout.textContent=`${dirs[Math.round(degrees/45)%8]}  ${String(Math.round(degrees)).padStart(3,'0')}°`;
    navHeading.textContent=dirs[Math.round(degrees/45)%8]+' VIEW';targetRange.textContent=`${Math.round(distance)}M`;objectiveCount.textContent=`${Math.round(distance)} M`;
    objectiveText.textContent=distance < 90 ? 'Relay mast visible. Close the final upland stretch.' : 'Follow the relay lock through the Ember Basin.';
    playerMarker.style.transform=`translate(-50%,-50%) rotate(${degrees+45}deg)`;
  }
  function update(delta) {
    if (!started || ended) return;
    elapsed=(performance.now()-startTime)/1000;
    if(keys.ArrowLeft)player.yaw-=delta*1.25;if(keys.ArrowRight)player.yaw+=delta*1.25;
    const forward=(keys.KeyW||keys.ArrowUp?1:0)-(keys.KeyS||keys.ArrowDown?1:0);
    const strafe=(keys.KeyD?1:0)-(keys.KeyA?1:0);
    const targetSpeed=forward*34;player.speed=lerp(player.speed,targetSpeed,Math.min(1,delta*4.2));
    const sy=Math.sin(player.yaw),cy=Math.cos(player.yaw);
    player.x+=sy*player.speed*delta+cy*strafe*22*delta;player.z+=cy*player.speed*delta-sy*strafe*22*delta;
    player.energy=clamp(player.energy+delta*19,0,100);player.firing=Math.max(0,player.firing-delta);
    for(const shot of shots) {
      shot.x+=shot.dx*178*delta;shot.y+=shot.dy*178*delta;shot.z+=shot.dz*178*delta;shot.life-=delta;
      if(shot.y<=heightAt(shot.x,shot.z)+.2){impact(shot);shot.life=0;}
    }
    shots=shots.filter(shot=>shot.life>0);
    for(const particle of particles){particle.x+=particle.vx*delta;particle.y+=particle.vy*delta;particle.z+=particle.vz*delta;particle.vy-=19*delta;particle.life-=delta;}
    particles=particles.filter(particle=>particle.life>0 && particle.y>heightAt(particle.x,particle.z)-1);
    if(Math.hypot(relay.x-player.x,relay.z-player.z)<19)finish();
    updateHud();
  }

  function resize() {
    dpr=Math.min(window.devicePixelRatio||1,2);W=window.innerWidth;H=window.innerHeight;canvas.width=Math.floor(W*dpr);canvas.height=Math.floor(H*dpr);canvas.style.width=`${W}px`;canvas.style.height=`${H}px`;
  }
  function frame(now) { const delta=Math.min(.04,(now-last)/1000);last=now;update(delta);render();requestAnimationFrame(frame); }

  deployButton.addEventListener('click',()=>{start();canvas.requestPointerLock?.();});
  restartButton.addEventListener('click',()=>{start();canvas.requestPointerLock?.();});
  canvas.addEventListener('click',()=>{if(started&&!ended){if(!pointerLocked)canvas.requestPointerLock?.();fire();}});
  document.addEventListener('pointerlockchange',()=>pointerLocked=document.pointerLockElement===canvas);
  document.addEventListener('mousemove',(event)=>{if(pointerLocked&&started&&!ended){player.yaw+=event.movementX*.0023;player.pitch=clamp(player.pitch-event.movementY*.0018,-.46,.30);}});
  window.addEventListener('keydown',(event)=>{keys[event.code]=true;if(event.code==='Space'){event.preventDefault();fire();}if(event.code==='Enter'&&!started)start();if(event.code==='KeyR'&&ended)start();});
  window.addEventListener('keyup',(event)=>{keys[event.code]=false;});
  window.addEventListener('resize',resize);

  setupGL();createWorld();reset();resize();requestAnimationFrame(frame);
})();
