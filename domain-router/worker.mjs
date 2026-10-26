export const PREFIX='/research/cp-100';
export const UPSTREAM='https://chladni-08-lab.vocallime128.chatgpt.site';
const PUBLIC_HEADERS=['accept','accept-language','if-none-match','if-modified-since','range','if-range'];

export function createHandler(fetchUpstream=fetch){
 return async function handle(request,env){
  const url=new URL(request.url);
  // The route wildcard also matches sibling slugs. Preserve the main site there.
  if(url.pathname!==PREFIX&&!url.pathname.startsWith(PREFIX+'/'))return env.WEBSITE.fetch(request);
  if(!['GET','HEAD'].includes(request.method))return new Response('Method not allowed',{status:405,headers:{Allow:'GET, HEAD'}});
  if(url.pathname===PREFIX){url.pathname+='/';return new Response(null,{status:308,headers:{Location:url.href,'Cache-Control':'no-store'}});}
  const target=new URL(UPSTREAM);target.pathname=url.pathname.slice(PREFIX.length);target.search=url.search;
  // House of EXP cookies, bearer tokens and staff Access headers stay on this origin.
  const headers=new Headers();for(const name of PUBLIC_HEADERS){const value=request.headers.get(name);if(value)headers.set(name,value);}
  try{
   const response=await fetchUpstream(target,{method:request.method,headers,redirect:'manual'});
   const output=new Response(request.method==='HEAD'?null:response.body,response);
   output.headers.delete('set-cookie');
   const location=output.headers.get('location');
   if(location){const next=new URL(location,target);if(next.origin===target.origin){next.protocol=url.protocol;next.host=url.host;next.pathname=PREFIX+next.pathname;output.headers.set('location',next.href);}}
   output.headers.set('X-Content-Type-Options','nosniff');
   return output;
  }catch{return new Response('Simulator temporarily unavailable',{status:502,headers:{'Cache-Control':'no-store'}});}
 };
}
export default {fetch:createHandler()};
