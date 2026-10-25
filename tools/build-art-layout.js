// SPDX-License-Identifier: GPL-3.0-only
// Measure visible artwork for consistent display sizing. Original PNGs are never changed.
const fs=require('node:fs'),path=require('node:path'),zlib=require('node:zlib'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'..'),layout={};
for(const system of require('../systems-data.js')){
  const png=fs.readFileSync(path.join(root,system.image));
  const width=png.readUInt32BE(16),height=png.readUInt32BE(20);
  if(png[24]!==8||png[25]!==6||png[28]!==0)throw Error('Expected non-interlaced RGBA8 artwork: '+system.id);
  const chunks=[];
  for(let offset=8;offset<png.length;){const length=png.readUInt32BE(offset),type=png.toString('ascii',offset+4,offset+8);if(type==='IDAT')chunks.push(png.subarray(offset+8,offset+8+length));offset+=length+12;}
  const data=zlib.inflateSync(Buffer.concat(chunks)),stride=width*4;
  if(data.length!==(stride+1)*height)throw Error('Unexpected PNG raster length');
  let previous=Buffer.alloc(stride),xmin=width,ymin=height,xmax=-1,ymax=-1;
  for(let y=0;y<height;y++){
    const row=Buffer.allocUnsafe(stride),filter=data[y*(stride+1)];
    for(let x=0;x<stride;x++){
      const a=x>=4?row[x-4]:0,b=previous[x],c=x>=4?previous[x-4]:0;
      let predictor=0;
      if(filter===1)predictor=a;else if(filter===2)predictor=b;else if(filter===3)predictor=(a+b)>>1;
      else if(filter===4){const p=a+b-c,pa=Math.abs(p-a),pb=Math.abs(p-b),pc=Math.abs(p-c);predictor=pa<=pb&&pa<=pc?a:pb<=pc?b:c;}
      else if(filter!==0)throw Error('Unknown PNG filter');
      row[x]=(data[y*(stride+1)+1+x]+predictor)&255;
    }
    for(let x=0;x<width;x++)if(row[x*4+3]>8){xmin=Math.min(xmin,x);xmax=Math.max(xmax,x);ymin=Math.min(ymin,y);ymax=Math.max(ymax,y);}
    previous=row;
  }
  if(xmax<0)throw Error('Empty console artwork: '+system.id);
  xmin=Math.max(0,xmin-12);ymin=Math.max(0,ymin-12);xmax=Math.min(width-1,xmax+12);ymax=Math.min(height-1,ymax+12);
  layout[system.id]={canvas:[width,height],bounds:[xmin,ymin,xmax-xmin+1,ymax-ymin+1],sha256:crypto.createHash('sha256').update(png).digest('hex')};
}
fs.writeFileSync(path.join(root,'assets/3d-icons/layout.js'),'// Generated display bounds only. PNG artwork is unchanged.\nconst shelfArtLayout = '+JSON.stringify(layout,null,2)+';\nif(typeof module!=="undefined")module.exports=shelfArtLayout;\n');
console.log('Measured visible display bounds for '+Object.keys(layout).length+' unchanged console PNGs.');
