const {test}=require('node:test');
const assert=require('node:assert/strict');
const vm=require('node:vm');
const fs=require('node:fs');
const source=fs.readFileSync('ui/app.js','utf8');
const storage=()=>{const data=new Map();return {getItem:k=>data.get(k)||null,setItem:(k,v)=>data.set(k,String(v)),removeItem:k=>data.delete(k)}};
function panel(local=storage(),session=storage(),options={}){
 const nodes=new Map(),timers=[];
 class Element{
  constructor(){this.children=[];this.dataset={};this.style={};this.value='';this.hidden=false;}
  set id(v){this._id=v;nodes.set(v,this)} get id(){return this._id}
  append(...items){this.children.push(...items)} replaceChildren(...items){this.children=items}
  get lastElementChild(){return this.children.at(-1)}
  setAttribute(){} querySelectorAll(){return []} click(){return this.onclick?.()}
 }
 for(const id of ['token','login','logout','status','master','master-learn','connect','error','tabs','main','meter-in','meter-out','meter-cpu','meter-latency','meter-xruns','meter-clips']){const n=new Element();n.id=id}
 const requests=[];
 const context=vm.createContext({window:{MF_META:{parameters:[]},localStorage:local,sessionStorage:session},localStorage:local,sessionStorage:session,document:{getElementById:id=>nodes.get(id),createElement:()=>new Element(),querySelectorAll:()=>[],querySelector:()=>null},setInterval:f=>timers.push(f),setTimeout:()=>0,console,fetch:async(url,opts)=>{
  requests.push({url,headers:opts.headers,body:opts.body});
  if(options.offline)throw Error('offline');
  const status=options.reject?503:200;
  const data=url.endsWith('/metadata')?{parameters:[]}:url.endsWith('/view')?{ids:[]}:{values:[],in:0,out:0,cpu:0,latencySamples:0,sampleRate:48000};
  return {ok:status===200,status,json:async()=>status===200?data:{error:'Token required'}};
 }});
 vm.runInContext(source,context);
 return {nodes,requests,timers,context,options};
}
const settle=()=>new Promise(r=>setImmediate(r));
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
 vm.runInContext("parameters=[{id:5,key:'midi.learn'}];window.MF_META.controlCount=100;",a.context);
 for(const id of [0,2,7,11,19,30,45,60,82,95]){
  await vm.runInContext(`learnButton(${id}).click()`,a.context);
  const request=a.requests.filter(r=>r.url.endsWith('/set')).at(-1);
  assert.deepEqual(JSON.parse(request.body),{id:5,value:(id+1)/100});
 }
});
