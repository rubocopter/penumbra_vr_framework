"""Compile the actual Overture narrative draw methods with a host font recorder.

No product build, renderer, game or headset is started. Tests keep production
method bodies intact and replace only the engine boundary.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

GAME = Path(__file__).resolve().parents[1] / 'PenumbraOverture'

def method(file, signature):
    source = (GAME / file).read_text()
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

STUB = r'''
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#define _W(x) L##x
namespace hpl {
using tWString=std::wstring;
using tWStringVec=std::vector<tWString>;
enum eFontAlign {eFontAlign_Left,eFontAlign_Right,eFontAlign_Center};
struct cVector2f {float x,y; cVector2f(float n=0):x(n),y(n){} cVector2f(float a,float b):x(a),y(b){} cVector2f operator*(float n)const{return {x*n,y*n};}};
struct cVector3f {float x,y,z; cVector3f(float a=0,float b=0,float c=0):x(a),y(b),z(c){} cVector3f operator+(const cVector3f&o)const{return{x+o.x,y+o.y,z+o.z};}};
struct cColor {float r,g,b,a; cColor(float v,float alpha):r(v),g(v),b(v),a(alpha){} cColor(float R,float G,float B,float A):r(R),g(G),b(B),a(A){}};
struct cMatrixf {int token=0;};
struct cMath {static cMatrixf MatrixMul(cMatrixf a,cMatrixf){return a;}};
struct Call {cVector3f pos;float width,line;cVector2f size;cColor color;eFontAlign align;tWString text;bool world;};
struct iFontData {
 std::vector<Call> calls;
 void GetWordWrapRows(float width,float,cVector2f size,const tWString& text,tWStringVec* rows){
  rows->clear();tWString row;float length=0;
  for(wchar_t ch:text){if(ch==L'\n'){rows->push_back(row);row.clear();length=0;}else{
   if(length+size.x*0.5f>width){rows->push_back(row);row.clear();length=0;}
   row+=ch;length+=size.x*0.5f;}}
  rows->push_back(row);
 }
 int DrawWordWrap(cVector3f p,float w,float l,cVector2f s,const cColor& c,eFontAlign a,const tWString&t){calls.push_back({p,w,l,s,c,a,t,false});return 1;}
 int DrawWordWrapWorld(cVector3f p,float w,float l,cVector2f s,const cColor&c,eFontAlign a,const tWString&t,cMatrixf){calls.push_back({p,w,l,s,c,a,t,true});return 1;}
 void Draw(cVector3f p,cVector2f s,const cColor&c,eFontAlign a,const wchar_t*t){calls.push_back({p,0,0,s,c,a,t,false});}
};
}
using namespace hpl;
struct Settings {float scale=1;float GetSubtitleScale(){return scale;}};
struct Inventory {float GetAlpha(){return 0.2f;}};
struct LowGfx {bool GetVREnabled(){return true;}};
struct cCamera3D {cMatrixf GetViewMatrix(){return {};}};
struct Scene {cCamera3D cam;cCamera3D* GetCamera(){return &cam;}};
struct Game {Scene scene;Scene* GetScene(){return &scene;}};
struct cRadioHandler;
struct cInit {Settings mVRSettings;bool mbHasHaptics=true,mbSubtitles=true;Inventory*mpInventory=nullptr;cRadioHandler*mpRadioHandler=nullptr;Game*mpGame=nullptr;};
struct cGameMessageHandler {cInit*mpInit;bool mbBlackText=false;};
struct cGameMessage {bool mbActive=true;float mfFade=0.35f;tWString msText=L"a\nb\nc";cGameMessageHandler*mpMessHandler;void Draw(iFontData*);};
struct cRadioHandler {cInit*mpInit;iFontData*mpFont;float mfAlpha=0.35f;tWString msCurrentText=L"a\nb\nc",msPrevText=L"old";bool active=false;bool IsActive(){return active;}void OnDraw();};
struct Image {float mfBrightness=0.35f;};
struct cIntroStory {cInit*mpInit;iFontData*mpFont;LowGfx*mpLowGfx;Image mvImages[6];tWString msCentreText,msMessage=L"a\nb\nc";void OnDraw();};
struct cSubTitle {float mfAlpha=0.5f;bool mbActive=true;tWString msMessage=L"a\nb\nc";cMatrixf mmTransform;};
using tSubTitleListIt=std::vector<cSubTitle>::iterator;
struct cEffect_SubTitle {cInit*mpInit;iFontData*mpFont;std::vector<cSubTitle> mlstSubTitles;void OnPostSceneDraw();};
'''
CHECKS = r'''
bool near(float a,float b){return std::abs(a-b)<0.001f;}
int main(){
 cInit init;Inventory inv;Game game;LowGfx low;iFontData font;
 init.mpInventory=&inv;init.mpGame=&game;
 cGameMessageHandler handler{&init};cGameMessage message;message.mpMessHandler=&handler;
 cRadioHandler radio{&init,&font};init.mpRadioHandler=&radio;
 cIntroStory intro{&init,&font,&low};cEffect_SubTitle effect{&init,&font};effect.mlstSubTitles.push_back({});
 for(float scale:{0.75f,1.0f,1.5f,2.0f}){
  init.mVRSettings.scale=scale;
  font.calls.clear();message.Draw(&font);
  if(font.calls.size()!=5)return 1;
  const auto m=font.calls[0];
  if(m.align!=eFontAlign_Center||!near(m.pos.x,400)||!near(m.pos.y,300-46*scale)||!near(m.size.x,28*scale)||!near(m.line,32*scale)||m.text!=message.msText||m.color.a!=0.35f){std::cerr<<"GameMessage narrative not centered/enlarged\n";return 2;}
  if(!near(font.calls[1].pos.x-m.pos.x,1)||!near(font.calls[1].pos.y-m.pos.y,1)||font.calls[1].pos.z!=150)return 3;
  handler.mbBlackText=true;font.calls.clear();message.Draw(&font);if(font.calls.size()!=1||font.calls[0].color.r!=0)return 4;handler.mbBlackText=false;
  font.calls.clear();radio.OnDraw();
  if(font.calls.size()!=4)return 5;
  const auto r=font.calls[0];if(r.align!=eFontAlign_Center||!near(r.pos.x,400)||!near(r.pos.y,300-49*scale)||!near(r.size.x,30*scale)||!near(r.line,34*scale)||r.color.a!=0.35f||font.calls[2].text!=L"old"||!near(font.calls[2].color.a,0.65f))return 6;
  if(!near(font.calls[1].pos.x-r.pos.x,2)||!near(font.calls[1].pos.y-r.pos.y,2)||font.calls[1].pos.z!=46)return 7;
  font.calls.clear();intro.OnDraw();
  if(font.calls.size()!=1)return 8;
  const auto v=font.calls[0];if(v.align!=eFontAlign_Center||!near(v.pos.x,400)||!near(v.pos.y,300-52*scale)||!near(v.size.x,32*scale)||!near(v.line,36*scale)||v.pos.z!=10)return 9;
  font.calls.clear();effect.OnPostSceneDraw();if(font.calls.size()!=3)return 10;
  const auto e=font.calls[2];if(!e.world||e.align!=eFontAlign_Center||!near(e.size.x,30*scale)||!near(e.line,32*scale)||!near(e.pos.x,0)||!near(e.pos.y,-47*scale)||!near(e.color.a,0.4f))return 11;
 }
 init.mbSubtitles=false;font.calls.clear();radio.OnDraw();intro.OnDraw();effect.OnPostSceneDraw();if(!font.calls.empty())return 12;
 init.mbSubtitles=true;message.mbActive=false;font.calls.clear();message.Draw(&font);if(!font.calls.empty())return 13;
 // Non-VR fallback stays native; inventory/notebook/credits are not draw consumers here.
 init.mbHasHaptics=false;font.calls.clear();radio.OnDraw();if(font.calls[0].align!=eFontAlign_Left||font.calls[0].pos.x!=40||font.calls[0].size.x!=30)return 14;
 std::cout<<"Overture GameMessage/Radio/video/world subtitles: scaled layout and native payload checks passed\n";
}
'''

class NarrativeIntegration(unittest.TestCase):
 def test_actual_narrative_draw_consumers(self):
  methods='\n'.join([method('GameMessageHandler.cpp','void cGameMessage::Draw('),method('RadioHandler.cpp','void cRadioHandler::OnDraw('),method('IntroStory.cpp','void cIntroStory::OnDraw('),method('EffectHandler.cpp','void cEffect_SubTitle::OnPostSceneDraw(')])
  helper=GAME/'NarrativeTextPresentation.h'
  source=STUB+ ('\n#include "NarrativeTextPresentation.h"\n' if helper.exists() else '')+methods+CHECKS
  output=Path(__file__).resolve().parents[3]/'work/narrative-text';output.mkdir(parents=True,exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='overture-host-',dir=output) as temp:
   directory=Path(temp).resolve();(directory/'integration.cpp').write_text(source)
   vcvars=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars32.bat'
   cmd=directory/'compile.cmd';cmd.write_text('@echo off\ncall "'+str(vcvars)+'" >nul\ncl /nologo /EHsc /std:c++14 /W3 /WX /I"'+str(GAME)+'" integration.cpp /Fe:integration.exe\n')
   run=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=directory,capture_output=True,text=True)
   self.assertEqual(run.returncode,0,run.stdout+run.stderr)
   run=subprocess.run([str(directory/'integration.exe')],capture_output=True,text=True)
   self.assertEqual(run.returncode,0,run.stdout+run.stderr)
   print(run.stdout.strip())
if __name__=='__main__':unittest.main()
