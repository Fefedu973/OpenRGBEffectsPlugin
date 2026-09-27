// SPDX-License-Identifier: GPL-2.0-or-later
// Optional synthetic oracle: reads the user's Better source tree without editing
// it, forces its documented WebGL test backend, and saves only generated fixtures.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {chromium}=require('playwright');
const root=path.resolve(process.argv[2]),out=path.resolve(process.argv[3]);
const fixtures=path.join(root,'tests/fixtures/native-rendering-v1');
const assets=path.join(root,'Better-SignalRGB-Screen-Capture/Services/WebOutput');
const cases=JSON.parse(fs.readFileSync(path.join(fixtures,'cases.json'))).cases.filter(c=>c.metadata.effectiveSettings.ambilightStyle==='Contours');
let script=fs.readFileSync(path.join(assets,'StreamingCanvasPage.js'),'utf8');
if(!script.includes('window.ContourHalo.create(contourCanvas);'))throw Error('Reference injection anchor changed');
script=script.replace('window.ContourHalo.create(contourCanvas);','window.ContourHalo.create(contourCanvas,{forceWebGL:true});')
 .replace(/\}\)\(\);\s*$/,'window.__appearanceMask=drawContourMask;})();');
const html=fs.readFileSync(path.join(assets,'StreamingCanvasPage.html'),'utf8')
 .replace('<!--CONTOUR_HALO_SCRIPT-->',()=>'<script>'+fs.readFileSync(path.join(assets,'ContourHalo.js'),'utf8')+'</script>')
 .replace('<!--WEB_OUTPUT_SCRIPT-->',()=>'<script>'+script+'</script>');
const clients=new Set();let current;
const part=(type,bytes,id)=>Buffer.concat([Buffer.from(`--frame\r\nContent-Type: ${type}\r\nContent-Length: ${bytes.length}\r\nX-State-Version: 1\r\n${id?`X-Source-Id: ${id}\r\n`:''}\r\n`),bytes,Buffer.from('\r\n')]);
const server=http.createServer((req,res)=>{
 if(req.url.startsWith('/web-stream')){
  const m=current.metadata,active=current.sources.filter(s=>m.sources.find(x=>x.id===s.id)?.hasFrame);
  res.writeHead(200,{'Content-Type':'multipart/x-mixed-replace; boundary=frame','Cache-Control':'no-store'});clients.add(res);res.on('close',()=>clients.delete(res));
  res.write(part('application/json',Buffer.from(JSON.stringify({version:1,canvasWidth:320,canvasHeight:200,outputWidth:m.outputWidth,outputHeight:m.outputHeight,settings:m.effectiveSettings,sources:active}))));
  for(const source of active)res.write(part('image/jpeg',fs.readFileSync(path.join(fixtures,`source-${current.pattern}.jpg`)),source.id));
 }else{res.writeHead(200,{'Content-Type':'text/html'});res.end(html);}
});
(async()=>{
 fs.mkdirSync(out,{recursive:true});await new Promise(r=>server.listen(0,'127.0.0.1',r));const browser=await chromium.launch({headless:true});const page=await browser.newPage();const diagnostics=[];
 try{for(const c of cases){current=c;const m=c.metadata;await page.setViewportSize({width:m.outputWidth,height:m.outputHeight});await page.goto(`http://127.0.0.1:${server.address().port}/${c.name}`);
  await page.waitForFunction(()=>window.webOutputDiagnostics?.renderCount>0&&window.webOutputDiagnostics.activeDecoders===0&&window.webOutputDiagnostics.pendingCount===0);await page.waitForTimeout(150);
  const result=await page.evaluate(m=>{const w=Math.min(320,m.outputWidth),h=Math.min(320,Math.max(1,Math.round(w*m.outputHeight/m.outputWidth)));const canvas=document.createElement('canvas');canvas.width=w;canvas.height=h;const ctx=canvas.getContext('2d'),s=m.effectiveSettings;ctx.setTransform(w/320,0,0,h/200,0,0);ctx.translate(s.screenX,s.screenY);ctx.scale(s.screenWidth/320,s.screenHeight/200);window.__appearanceMask(ctx);return {mask:canvas.toDataURL('image/png').split(',')[1],diagnostics:window.webOutputDiagnostics.halo};},m);
  fs.writeFileSync(path.join(out,c.name+'-placed-mask.png'),Buffer.from(result.mask,'base64'));await page.screenshot({path:path.join(out,c.name+'-webgl.png'),animations:'disabled'});diagnostics.push({name:c.name,...result.diagnostics});
  if(result.diagnostics?.backend!=='webgl')throw Error('WebGL reference did not run: '+JSON.stringify(result.diagnostics));
 }fs.writeFileSync(path.join(out,'webgl-reference.json'),JSON.stringify({browser:await browser.version(),cases:diagnostics},null,2));console.log('PASS synthetic production WebGL references: '+diagnostics.length);
 }finally{await browser.close();for(const response of clients)response.destroy();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
