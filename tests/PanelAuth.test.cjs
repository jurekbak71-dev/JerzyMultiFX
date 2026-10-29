const {test}=require('node:test');
const assert=require('node:assert/strict');
const vm=require('node:vm');
const fs=require('node:fs');
const source=fs.readFileSync('ui/app.js','utf8');
const storage=()=>{const data=new Map();return {getItem:k=>data.get(k)||null,setItem:(k,v)=>data.set(k,String(v)),removeItem:k=>data.delete(k)}};
function panel(local=storage(),session=storage(),options={}){
 const nodes=new Map(),timers=[],roots=[],timeouts=new Map();let timeoutId=0;
 const parameters=options.parameters||['master.bypass',...Array.from({length:5},(_,i)=>'block.'+i+'.bypass')].map((key,id)=>({id,key,name:key,label:key,default:0,min:0,max:1,steps:1,control:'switch'}));
 const values=parameters.map(p=>p.default||0);
 const matches=(x,selector)=>selector==='button'?x.tagName==='button':selector==='[data-parameter]'?x.dataset.parameter!==undefined:selector==='[data-block]'?x.dataset.block!==undefined:false;
 const walk=(root,selector)=>root.children.flatMap(x=>[...(matches(x,selector)?[x]:[]),...walk(x,selector)]);
 class Element{
  constructor(tag){this.tagName=tag;this.children=[];this.dataset={};this.style={};this.value='';this.hidden=false;this.attributes={};}
  set id(v){this._id=v;nodes.set(v,this)} get id(){return this._id}
  append(...items){this.children.push(...items)} replaceChildren(...items){this.children=items}
  get lastElementChild(){return this.children.at(-1)}
  setAttribute(k,v){this.attributes[k]=v} querySelectorAll(selector){return walk(this,selector)} click(){return this.onclick?.()}
 }
 for(const id of ['token','login','logout','status','master','master-learn','connect','error','tabs','main','meter-in','meter-out','meter-cpu','meter-latency','meter-xruns','meter-clips']){const n=new Element();n.id=id;roots.push(n)}
 const requests=[];
 const document={getElementById:id=>nodes.get(id),createElement:tag=>new Element(tag),querySelectorAll:selector=>[...new Set(roots.flatMap(r=>[...(matches(r,selector)?[r]:[]),...walk(r,selector)]))],querySelector:()=>null};
 const context=vm.createContext({window:{MF_META:{parameters:[]},localStorage:local,sessionStorage:session},localStorage:local,sessionStorage:session,document,setInterval:f=>timers.push(f),setTimeout:f=>{timeouts.set(++timeoutId,f);return timeoutId},clearTimeout:id=>timeouts.delete(id),console,fetch:async(url,opts)=>{
  requests.push({url,headers:opts.headers,body:opts.body});
  if(options.offline)throw Error('offline');
  const status=options.reject?503:200;
  if(url.endsWith('/set')){if(options.deferSet)await options.deferSet;if(options.failSet)return {ok:false,status:503,json:async()=>({error:'offline'})};const v=JSON.parse(opts.body);values[v.id]=v.value;}
  if(url.endsWith('/state')&&options.deferState)await options.deferState;
  const data=url.endsWith('/metadata')?{parameters,controlCount:parameters.length}:url.endsWith('/view')?{ids:options.ids||[]}:{values:[...values],in:0,out:0,cpu:0,latencySamples:0,sampleRate:48000};
  return {ok:status===200,status,json:async()=>status===200?data:{error:'Token required'}};
 }});
 vm.runInContext(source,context);
 return {nodes,requests,timers,context,options,document,values,timeouts};
}
const settle=()=>new Promise(r=>setImmediate(r));
test('compressor MAKEUP displays attenuation, zero and boost on both panels',()=>{
 const a=panel();
 for(const key of ['comp.makeup','delayA.oil.icmakeup','delayA.oil.ocmakeup',
                   'delayB.oil.icmakeup','delayB.oil.ocmakeup']){
  const p={key,min:-24,max:24};
  for(const [value,label] of [[0,'-24.0 dB'],[.5,'+0.0 dB'],[1,'+24.0 dB']]){
   a.context.p=p;a.context.v=value;
   assert.equal(vm.runInContext('formatted(p,v)',a.context),label);
  }
 }
 a.context.p={key:'td.optoMakeup',min:0,max:1};
 a.context.v=0;assert.equal(vm.runInContext('formatted(p,v)',a.context),'-12.0 dB');
 a.context.v=.5;assert.equal(vm.runInContext('formatted(p,v)',a.context),'+0.0 dB');
 a.context.p={key:'attack.makeup',min:-12,max:12};
 a.context.v=0;assert.equal(vm.runInContext('formatted(p,v)',a.context),'-12.0 dB');
 a.context.v=.5;assert.equal(vm.runInContext('formatted(p,v)',a.context),'+0.0 dB');
 a.context.v=1;assert.equal(vm.runInContext('formatted(p,v)',a.context),'+12.0 dB');
});
test('panel opens automatically without a token or browser storage',async()=>{
 const a=panel();await settle();await settle();
 assert.equal(a.requests[0].url,'/');assert.equal(a.nodes.get('error').textContent,'');
 assert.equal(vm.runInContext('connected',a.context),true);
 assert.equal(a.requests.find(r=>r.url.endsWith('/metadata')).headers.Authorization,undefined);
 assert.equal(a.nodes.get('main').children[0].id,'blocks');
});
test('core starting late retries automatically',async()=>{
 const a=panel(storage(),storage(),{reject:true});await settle();
 assert.equal(vm.runInContext('connected',a.context),false);
 a.options.reject=false;await vm.runInContext('connectPanel()',a.context);
 assert.equal(vm.runInContext('connected',a.context),true);
});
test('MIDI Learn for every switch sends its own parameter ID',async()=>{
 const a=panel();await settle();await settle();
 vm.runInContext("parameters.push({id:6,key:'midi.learn',name:'MIDI Learn'});window.MF_META.controlCount=100;",a.context);
 for(const id of [0,2,7,11,19,30,45,60,82,95]){
  await vm.runInContext(`learnButton(${id}).click()`,a.context);
  const request=a.requests.filter(r=>r.url.endsWith('/set')).at(-1);
  assert.deepEqual(JSON.parse(request.body),{id:6,value:(id+1)/100});
 }
});
test('focused switch responds before HTTP acknowledgement and rolls back on failure',async()=>{
 const parameters=[{id:0,key:'master.bypass',name:'Master',min:0,max:1,steps:1,default:0,control:'switch'}];
 const a=panel(storage(),storage(),{parameters});await settle();await settle();
 let release;a.options.deferSet=new Promise(r=>release=r);
 const button=a.nodes.get('master');a.document.activeElement=button;
 const click=button.click();await settle();
 assert.equal(button.textContent,'WYŁĄCZONY / BYPASS');assert.equal(button.className,'toggle off');assert.equal(button.attributes['aria-busy'],'true');
 release();await click;assert.equal(button.attributes['aria-busy'],'false');
 a.options.deferSet=null;a.options.failSet=true;await button.click();
 assert.equal(button.textContent,'WYŁĄCZONY / BYPASS');assert.equal(button.className,'toggle off');
});
test('MIDI updates focused master and block switches without rebuilding the panel',async()=>{
 const a=panel();await settle();await settle();
 const before=a.nodes.get('main').children[0],button=a.nodes.get('master');a.document.activeElement=button;
 a.values[0]=1;a.values[1]=1;await a.timers[0]();
 assert.equal(button.className,'toggle off');assert.equal(button.textContent,'WYŁĄCZONY / BYPASS');
 const block=a.document.querySelectorAll('[data-parameter]').find(x=>Number(x.dataset.parameter)===1);
 assert.equal(block.className,'toggle off');assert.equal(a.nodes.get('main').children[0],before);
 await block.click();await block.click();assert.equal(block.textContent,'WYŁĄCZONY / BYPASS');
});
test('an older poll cannot undo a newer switch click',async()=>{
 const a=panel();await settle();await settle();let release;
 a.options.deferState=new Promise(r=>release=r);
 const poll=a.timers[0]();await settle();await a.nodes.get('master').click();
 a.values[0]=0;release();await poll;
 assert.equal(a.nodes.get('master').textContent,'WYŁĄCZONY / BYPASS');
});
test('all discrete parameters render as switches, lists or numeric fields',async()=>{
 const a=panel();await settle();await settle();
 for(const [steps,control,choices] of [[1,'switch',''],[2,'select','A|B|C'],[3,'select',''],[65534,'number','']]){
  const key=JSON.stringify({id:20,key:'test.option',name:'Test',min:0,max:steps,steps,control,choices});
  const element=vm.runInContext(`control(${key})`,a.context);
  assert.notEqual(element.children[0]?.className,'rotary');
  if(control==='switch')assert.equal(element.children[0].tagName,'button');
  if(control==='select')assert.equal(element.children[0].tagName,'select');
  if(control==='number')assert.equal(element.children[0].type,'number');
 }
});
test('delay bank MIX is a compact slider and Reel ON/OFF is a switch',async()=>{
 const a=panel();await settle();await settle();
 const mix={id:20,key:'delayA.mix',name:'Bank mix',label:'BANK MIX',min:0,max:1,default:.5,steps:0,control:'knob'};
 const enabled={id:21,key:'delayA.reel.enabled',name:'Reel on',min:0,max:1,default:1,steps:1,control:'switch'};
 vm.runInContext('values[21]=1',a.context);
 const slider=vm.runInContext(`control(${JSON.stringify(mix)},true)`,a.context);
 const switchWidget=vm.runInContext(`control(${JSON.stringify(enabled)},true)`,a.context);
 assert.equal(slider.children[0].type,'range');
 assert.equal(slider.children[0].dataset.kind,'mini');
 assert.equal(switchWidget.children[0].tagName,'button');
 assert.equal(switchWidget.children[0].textContent,'WŁĄCZONY');
});
test('DELAY B MX controls identify their bank and internal engine',async()=>{
 const a=panel();await settle();await settle();
 const label=(key,name,title=name.split(' / ').pop())=>vm.runInContext(`name(${JSON.stringify({key,name,label:title})})`,a.context);
 assert.equal(label('delayA.mx.slot0_time','delayA.mx / Silnik A: Czas'),'DELAY A / MX 1: Czas');
 assert.equal(label('delayB.mx.slot0_time','delayB.mx / Silnik A: Czas'),'DELAY B / MX 1: Czas');
 assert.equal(label('delayB.mx.slot1_feedback','delayB.mx / Silnik B: Sprzężenie'),'DELAY B / MX 2: Sprzężenie');
 assert.equal(label('delayB.mx.slot0_time','delayB.mx / Silnik A: Czas','DELAY B / MX 1: Czas'),'DELAY B / MX 1: Czas');
});
test('continuous knob and compact slider send during movement, not only on release',async()=>{
 const base=['master.bypass',...Array.from({length:5},(_,i)=>'block.'+i+'.bypass')].map((key,id)=>({id,key,name:key,default:0,min:0,max:1,steps:1,control:'switch'}));
 const gain={id:6,key:'td.sat',name:'Saturation',min:0,max:1,default:0,steps:0,control:'knob'};
 for(const compact of [false,true]){
  const a=panel(storage(),storage(),{parameters:[...base,gain]});await settle();await settle();
  const widget=vm.runInContext(`control(${JSON.stringify(gain)},${compact})`,a.context);
  const input=compact?widget.children[0]:widget.children[0].children[1];
  for(let i=1;i<=10;i++){input.value=i/10;input.oninput()}
  assert.equal(a.timeouts.size,1,'continuous movement must not keep deferring the timer');
  const [id,callback]=a.timeouts.entries().next().value;a.timeouts.delete(id);callback();await settle();
  assert.deepEqual(JSON.parse(a.requests.filter(r=>r.url.endsWith('/set')).at(-1).body),{id:6,value:1});
  input.value=.3;input.oninput();assert.equal(a.timeouts.size,1,'the next movement schedules the next live update');
 }
});
test('child enabled state cannot hide a bypassed master or parent block',async()=>{
 const a=panel();await settle();await settle();
 a.values[0]=1;await a.timers[0]();
 assert.equal(a.nodes.get('signal-status').hidden,false);assert.match(a.nodes.get('signal-status').textContent,/CAŁY TOR WYŁĄCZONY/);
 a.values[0]=0;a.values[1]=1;await a.timers[0]();
 assert.match(a.nodes.get('signal-status').textContent,/ANALOG WYŁĄCZONY/);
 a.values[1]=0;await a.timers[0]();assert.equal(a.nodes.get('signal-status').hidden,true);
});
