/*
 *  Copyright 2011, 2012 DFKI GmbH Robotics Innovation Center
 *
 *  This file is part of the MARS simulation framework.
 *
 *  MARS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3
 *  of the License, or (at your option) any later version.
 *
 *  MARS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with MARS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//#include "HUD.h"
#include "GraphicsManager.h"

#include <iostream>
#include <string>

#include <osgViewer/ViewerEventHandlers>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/TerrainManipulator>
#include <osgWidget/Frame>


#include <stdexcept>

#include "GraphicsWindow.h"

#define CULL_LAYER (1 << (widgetID-1))


namespace osgviz {
  namespace graphics {

    using namespace std;
    using namespace interfaces;
    using std::cout;
    using std::cerr;
    using std::endl;

    template<class T>
    class map_data_compare : public std::binary_function<typename T::value_type,
                                                         typename T::mapped_type,
                                                         bool>
    {
    public:
      bool operator() (typename T::value_type &pair,
                       typename T::mapped_type i) const
      {
        return pair.second == i;
      }
    };


    GraphicsWindow::GraphicsWindow(void *parent,
                                   osg::Group* scene, unsigned long id,
                                   bool isRTTWidget, int f,
                                   GraphicsManager *gm):
      _osgWidgetWindowManager(0), _osgWidgetWindowCnt(0), gm(gm) {
      (void)f;
      (void)parent;

      widgetID = id;

      this->isRTTWidget = isRTTWidget;
      isStereoDisplay = isFullscreen = false;
      isMouseMoving = isMouseButtonDown = false;
      isHUDShown = true;
      widgetX = 20;
      widgetY = 50;
      widgetWidth = 720;
      widgetHeight = 405;

      graphicsWindow = 0;
//      myHUD = 0;
//      hudCamera = 0;
      graphicsCamera = 0;

      cameraEyeSeparation = 0.1;
      mouseX = mouseY = 0;
      pickmode = DISABLED;

      this->scene = scene;
      view = new osgViewer::View;
    }

    GraphicsWindow::~GraphicsWindow() {
      /* if the destructor is called from somewhere else than osg
       * (e.g. from the QWidget) we have to increment the referece counter
       * to prevent osg from calling the destructor one more time.
       */
      this->ref();
      if(gm) gm->removeGraphicsWidget(widgetID);
      delete graphicsCamera;

    }

    int GraphicsWindow::addOsgWindow(osgWidget::Window* wnd){
      this->_osgWidgetWindowCnt++;
      int id= _osgWidgetWindowCnt;

      osg::ref_ptr<osgWidget::Window> w = getWindowById(id);
      if(w.valid()){
        cout << "id is already in MAP !! error!!" << endl;
        return -1;
      }else {
        _osgWindowIdMap.insert(WindowIdMapType::value_type(id,wnd ));
        cout << "added window to the map: " << id << endl;
      }

      return id;
    }


    bool GraphicsWindow::setFont(int id,const std::string &fontname){
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        osgWidget::Label* label = dynamic_cast<osgWidget::Label*>(wnd.get());
        if(label){
          label->setFont(fontname);
          return true;
        }
      }
      return false;
    }

    bool GraphicsWindow::setFontColor(int id,float r, float g,float b,float a){
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        osgWidget::Label* label = dynamic_cast<osgWidget::Label*>(wnd.get());
        if(label){
          label->setFontColor(r,g,b,a);
          return true;
        }
      }
      return false;
    }

    bool GraphicsWindow::setFontSize(int id,int size){
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        osgWidget::Label* label = dynamic_cast<osgWidget::Label*>(wnd.get());
        if(label){
          label->setFontSize(size);
          return true;
        }
      }
      return false;
    }

    bool GraphicsWindow::createStyle(const std::string& name,const std::string &style){

      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();
      if(wm.valid()){
        cout << "GraphicsWidget::createStyle: " << style << endl;
        return  wm->getStyleManager()->addStyle(new osgWidget::Style(name, style));
      }

      return false;

    }
    bool GraphicsWindow::setSize(int id,float x, float y){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->setSize(x,y);
          return true;
        }
      }else{
        wnd->resize(x,y);
        return true;
      }
      return false;

    }


    bool GraphicsWindow::setStyle(int id,const std::string &styleName){
      cerr << " GraphicsWidget::setStyle " << styleName << endl;
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          cerr << " GraphicsWidget::setStyle SET IT !" << endl;
          wd->setStyle(styleName);
          return true;
        }
      }else{
        cerr << " GraphicsWidget::setStyle SET IT !" << endl;
        wnd->setStyle(styleName);
        return true;
      }
      return false;
    }

    bool GraphicsWindow::hideWindow(int wndId){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();

      if(wnd.valid() && wm.valid()){
        return wm->removeChild(wnd);
      }

      return false;
    }
    bool GraphicsWindow::deleteWidget(int wdgId)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(wdgId);
      if(wd.valid()){
        _osgWidgetIdMap.erase(wdgId);
        WidgetCallBackMapType::iterator it = _widgetCallBackMap.find(wdgId);
        if(it != _widgetCallBackMap.end()){
          _widgetCallBackMap.erase(it);
        }

        return true;
      }

      return false;
    }

    bool GraphicsWindow::deleteWindow(int wndId){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();
      if(wnd.valid() && wm.valid()){
        wm->removeChild(wnd);

        const osgWidget::Window::Vector& v = wnd->getObjects();
        for(unsigned int i = 0;i<v.size();i++){
          osgWidget::Widget* w = v[i].get();
          WidgetIdMapType::iterator it = std::find_if( _osgWidgetIdMap.begin(), _osgWidgetIdMap.end(), std::bind2nd(map_data_compare<WidgetIdMapType>(), w) );

          if ( it != _osgWidgetIdMap.end() ){
            _osgWidgetIdMap.erase(it);
            cerr << "erasing widget" << endl;
          }

        }

        _osgWindowIdMap.erase(wndId);
      }

      return false;
    }
    int GraphicsWindow::createInput(const std::string& name, const std::string& text, int count)
    {
      osg::ref_ptr<osgWidget::Label> widget =   new osgWidget::Input(name,text,count);
      return addOsgWidget(widget);
    }

    int GraphicsWindow::createLabel(const std::string &name,const std::string &text){

      osg::ref_ptr<osgWidget::Label> widget =   new osgWidget::Label(name,text);
      return addOsgWidget(widget);
    }

    bool GraphicsWindow::setAnchorVertical(int id,int va){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(wnd.valid()){
        wnd->setAnchorVertical( (osgWidget::Window::VerticalAnchor) (va+1) );
        return true;
      }
      return false;
    }

    bool GraphicsWindow::setAnchorHorizontal(int id,int ha){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(wnd.valid()){
        wnd->setAnchorHorizontal((osgWidget::Window::HorizontalAnchor) (ha+1));
        return true;
      }
      return false;
    }

    bool GraphicsWindow::setAlignHorizontal(int id,int h){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setAlignHorizontal((osgWidget::Widget::HorizontalAlignment)h);
        return true;
      }
      return false;
    }
    bool GraphicsWindow::setAlignVertical(int id, int v){

      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setAlignVertical( (osgWidget::Widget::VerticalAlignment) v);
        return true;
      }
      return false;
    }
    bool GraphicsWindow::getAlignHorizontal(int id,int &h){

      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        h = wd->getAlignHorizontal();
        return true;
      }
      return false;
    }
    bool GraphicsWindow::getAlignVertical(int id, int &v){

      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        v = wd->getAlignVertical();
        return true;
      }
      return false;
    }

    bool GraphicsWindow::getLayer(int id, int& layer)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        layer = wd->getLayer();
        return true;
      }
      return false;
    }
    bool GraphicsWindow::setLayer(int id, int layer, int offset)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setLayer((osgWidget::Widget::Layer) layer,offset);
        return true;
      }
      return false;
    }



    bool GraphicsWindow::addWidgetToWindow(int window, int widget, int x, int y)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(window);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(widget);

      if(wnd.valid() && wd.valid()){
        osgWidget::Table* table = dynamic_cast<osgWidget::Table*>(wnd.get());
        if(table){
          cout << "added WIDGET TO TABLE.. " << x << " " << y << endl;
          table->addWidget(wd,x,y);
          return true;
        }else{
          osgWidget::Canvas* can = dynamic_cast<osgWidget::Canvas*>(wnd.get());
          if(can){
            cout << "added WIDGET TO CANVAS.." << endl;
            can->addWidget(wd,x,y);
            return true;
          }else{
            osgWidget::Frame* frame = dynamic_cast<osgWidget::Frame*>(wnd.get());
            if(frame){
              osg::ref_ptr<osgWidget::Window> content = getWindowById(widget);
              if(content.valid()){
                frame->setWindow(content);
              }else{
                cerr << "You spezified a Frame as Window, ";
                if(wd.valid()){
                  cerr << "and a widget as content, but a Frame takes an other Window as content" << endl;
                }else{
                  cerr << "but other Window as content is not valid" << endl;
                }

              }

            }else{
              cerr << "UNKNOWN WINODW TYPE" << endl;
            }
          }
        }
        cout << "wnd is not type canvas or !." << endl;
      }
      return false;
    }

    bool GraphicsWindow::addWidgetToWindow(int window,int widget,float x, float y){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(window);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(widget);

      if(wnd.valid() && wd.valid()){
        osgWidget::Canvas* can = dynamic_cast<osgWidget::Canvas*>(wnd.get());
        if(can){
          can->addWidget(wd,x,y);
          return true;
        }
        cout << "wnd is not type canvas!." << endl;
      }
      return false;
    }

    bool GraphicsWindow::addWidgetToWindow(int window, int widget)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(window);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(widget);

      if(wnd.valid() && wd.valid()){
        wnd->addWidget(wd);
        cout << "added widget to window." << endl;
        return true;
      }

      cout << "window or widget does not exist!" << endl;
      return false;
    }

    int GraphicsWindow::addOsgWidget(osgWidget::Widget *wid){
      this->_osgWidgetWindowCnt++;
      int id= _osgWidgetWindowCnt;
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        cout << "id is already in MAP !! error!!" << endl;
        return -1;
      }else{
        _osgWidgetIdMap.insert(WidgetIdMapType::value_type(id,wid ));
        cout << "widget created." << id << endl;
      }
      return id;
    }
    int GraphicsWindow::createWidget(const std::string &name,float sizex,float sizey){
      osg::ref_ptr<osgWidget::Widget> widget =   new osgWidget::Widget(name,sizex,sizey);
      return addOsgWidget(widget.get());

    }

    bool GraphicsWindow::setColor(int id, float r, float g, float b, float a)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->setColor(r,g,b,a);
          return true;
        }
      }else{
        wnd->getBackground()->setColor(r,g,b,a);
        return true;
      }
      return false;
    }

    osgWidget::Widget* GraphicsWindow::getWidgetById(int wdId){
      WidgetIdMapType::iterator iter= _osgWidgetIdMap.find(wdId);
      if(iter != _osgWidgetIdMap.end() ){
        return (*iter).second.get();
      }
      return NULL;
    }

    osgWidget::Window* GraphicsWindow::getWindowById(int wndId){
      WindowIdMapType::iterator iter= _osgWindowIdMap.find(wndId);
      if(iter != _osgWindowIdMap.end() ){
        return (*iter).second.get();
      }

      return NULL;

    }
    bool GraphicsWindow::windowSetPosition(int wndId, float x, float y)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      if(wnd.valid()){
        wnd->setPosition(osgWidget::Point(x,y,0));
        return true;
      }
      return false;
    }

    bool GraphicsWindow::showWindow(int wndId){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();
      if(wnd.valid() && wm.valid()){
        if(! wm->getByName(wnd->getName() )){// not already insered..
          wm->addChild(wnd);
        }
      }else{
        cout << "window not known!" << endl;
        return false;
      }

      return true;

    }

    bool GraphicsWindow::setCanFill(int id, bool state){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setCanFill(state);
        return true;
      }
      return false;
    }
    bool GraphicsWindow::setShadow(int id,float intensity){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        osgWidget::Label *label = dynamic_cast<osgWidget::Label*>( wd.get() );
        if(label){
          label->setShadow(intensity);
          return true;
        }
      }
      return false;
    }
    bool GraphicsWindow::addSize(int id, float x, float y){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->addSize(x,y);
        return true;
      }
      return false;
    }

    bool GraphicsWindow::addColor(int id,float r,float g,float b,float a){
      cerr << "in add color" << endl;

      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->addColor(r,g,b,a);
          cerr << "add color:r" << r << " g:" << g <<" b:" << b << endl;
          return true;
        }
      }else{
        wnd->getBackground()->addColor(r,g,b,a);
        return true;
      }
      return false;
    }

    bool GraphicsWindow::setLabel(int id, const std::string& text)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        osgWidget::Label *label = dynamic_cast<osgWidget::Label*>( wd.get() );
        if(label){
          label->setLabel(text);
          return true;
        }
      }
      return false;
    }

    bool GraphicsWindow::manageClickEvent(osgWidget::Event& event)
    {

      cerr << "seaching 4 callback!" << endl;

      WidgetCallBackPairType *ip =(WidgetCallBackPairType*) event.getData();

      if(ip){
#ifdef NO_TR1
        if(ip->second == nullptr && ip->first) {
#else
        if(ip->second == NULL && ip->first) {
#endif
          guiClickCallBack  call=  ip->first;
          call(event.x ,event.y);

        }else{
          if(ip->second.get()) {
            (*(ip->second.get()))(event.x ,event.y);
          }
          cerr << "base set" << endl;
        }
      }

      return false;
    }

    bool GraphicsWindow::setImage(int id, const std::string& path)
    {
      osg::ref_ptr<osg::Image> image = osgDB::readImageFile(path);
      if(!image.valid()){
        return false;
      }

      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);

      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->setImage(image.get(),true);
          return true;
        }
      }else{
        wnd->getBackground()->setImage(image,true);
        return true;
      }
      return false;
    }



    bool GraphicsWindow::addEventToWidget(int id,guiClickCallBack function,guiClickCallBackBind *bindptr, osgWidget::EventType type){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);


      if(wnd.valid() || wd.valid()){
        cout << "found somethink to add callback" << endl;
        WidgetCallBackList *wcb;
        WidgetCallBackMapType::iterator it =  _widgetCallBackMap.find(id);
        if(it != _widgetCallBackMap.end()){
          wcb = &it->second;
        }else{
          _widgetCallBackMap.insert(WidgetCallBackMapType::value_type(id,WidgetCallBackList() ) );
          wcb = & _widgetCallBackMap.find(id)->second ;
        }
#ifdef NO_TR1
        wcb->push_back(WidgetCallBackPairType(function, std::shared_ptr<guiClickCallBackBind>(bindptr) ));
#else
        wcb->push_back(WidgetCallBackPairType(function, std::tr1::shared_ptr<guiClickCallBackBind>(bindptr) ));
#endif
        if(wnd.valid()){
          wnd->setEventMask( wnd->getEventMask()| type);
          wnd->addCallback( new osgWidget::Callback(&GraphicsWindow::manageClickEvent,this, type,(void*)&wcb->back()));

        }else{
          cout << "addCallback to Widget !!" << endl;
          wd->setEventMask(osgWidget::EVENT_ALL);
          wd->addCallback( new osgWidget::Callback(&GraphicsWindow::manageClickEvent,this, type,(void*)&wcb->back()));


        }
        return true;

      }
      return false;
    }

    bool GraphicsWindow::addMousePushEventCallback(int id, guiClickCallBack function,guiClickCallBackBind *bindptr){
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_PUSH);
    }
    bool GraphicsWindow::addMouseReleaseEventCallback(int id, guiClickCallBack function,guiClickCallBackBind *bindptr){
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_RELEASE);
    }
    bool GraphicsWindow::addMouseEnterEventCallback(int id, GraphicsGuiInterface::guiClickCallBack function, guiClickCallBackBind* bindptr)
    {
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_ENTER);
    }
    bool GraphicsWindow::addMouseLeaveEventCallback(int id, guiClickCallBack function,guiClickCallBackBind *bindptr){
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_LEAVE);
    }

    int GraphicsWindow::createCanvas(const std::string& name)
    {
      cout << "createCanvas" << endl;
      osg::ref_ptr<osgWidget::Window>  wnd = new osgWidget::Canvas(name);
      return addOsgWindow(wnd);
    }
    int GraphicsWindow::createTable(const std::string& name, int row, int colums)
    {
      osg::ref_ptr<osgWidget::Window>  wnd = new osgWidget::Table(name,row,colums);
      return addOsgWindow(wnd);

    }

    int GraphicsWindow::createFrame(const std::string& name, float x1, float y1, float x2, float y2)
    {
      osg::ref_ptr<osgWidget::Frame> frame = osgWidget::Frame::createSimpleFrame(
                                                                                 name,
                                                                                 x1,
                                                                                 y1,
                                                                                 x2,
                                                                                 y2
                                                                                 );

      return addOsgWindow(frame);
    }

    int GraphicsWindow::createBox(const std::string& name,int type){

      cout << "createBox" << endl;
      osg::ref_ptr<osgWidget::Window>  wnd = new osgWidget::Box(name,type);
      return addOsgWindow(wnd);
    }

    osgWidget::WindowManager* GraphicsWindow::getOrCreateWindowManager()
    {
      if(this->_osgWidgetWindowManager){
        return _osgWidgetWindowManager;
      }

      osgWidget::point_type w = view->getCamera()->getViewport()->width();
      osgWidget::point_type h =  view->getCamera()->getViewport()->height();


      osgWidget::WindowManager* wm = new osgWidget::WindowManager(
                                                                  view,
                                                                  w,
                                                                  h,
                                                                  CULL_LAYER,
                                                                  osgWidget::WindowManager::WM_PICK_DEBUG| osgWidget::WindowManager::WM_USE_PYTHON
                                                                  );

      osg::Camera* camera = wm->createParentOrthoCamera();



      view->addEventHandler(new osgWidget::MouseHandler(wm));
      view->addEventHandler(new osgWidget::KeyboardHandler(wm));
      view->addEventHandler(new osgWidget::ResizeHandler(wm, camera));
      view->addEventHandler(new osgWidget::CameraSwitchHandler(wm, camera));
      view->addEventHandler(new osgViewer::StatsHandler());
      view->addEventHandler(new osgViewer::WindowSizeHandler());
      view->addEventHandler(new osgGA::StateSetManipulator(
                                                           view->getCamera()->getOrCreateStateSet()
                                                           ));

      view->getCamera()->addChild( camera );
      _osgWidgetWindowManager = wm;

      return wm;

    }

    osg::ref_ptr<osg::GraphicsContext> GraphicsWindow::createWidgetContext(
                                                                           void* parent,
                                                                           osg::ref_ptr<osg::GraphicsContext::Traits> traits) {
      //traits->windowDecoration = false;

      osg::DisplaySettings* ds = osg::DisplaySettings::instance();
      if (ds->getStereo()) {
        switch(ds->getStereoMode()) {
        case(osg::DisplaySettings::QUAD_BUFFER):
          traits->quadBufferStereo = true;
          break;
        case(osg::DisplaySettings::VERTICAL_INTERLACE):
        case(osg::DisplaySettings::CHECKERBOARD):
        case(osg::DisplaySettings::HORIZONTAL_INTERLACE):
          traits->stencil = 8;
          break;
        default: break;
        }
      }

      osg::ref_ptr<osg::GraphicsContext> gc =
        osg::GraphicsContext::createGraphicsContext(traits.get());

      traits->x = 0;
      traits->y = 0;
      traits->width = 1920;
      traits->height = 1080;

      return gc;
    }

    void GraphicsWindow::initializeOSG(void* data,
                                       GraphicsWindow* shared, int width, int height) {
      osg::ref_ptr<osgGA::KeySwitchMatrixManipulator> keyswitchManipulator;

      if(width > 0) widgetWidth = width;
      if(height > 0) widgetHeight = height;

      // do not use osg default lighting
      view->setLightingMode(osg::View::NO_LIGHT);

      createContext(data, shared, widgetWidth, widgetHeight);
      if(!isRTTWidget) {
        graphicsWindow->setWindowName("3D Environment");

        keyswitchManipulator = new osgGA::KeySwitchMatrixManipulator;

        keyswitchManipulator->addMatrixManipulator( '1', "Terrain", new osgGA::TerrainManipulator() );
        keyswitchManipulator->addMatrixManipulator( '2', "Trackball", new osgGA::TrackballManipulator() );
        keyswitchManipulator->addMatrixManipulator( '3', "Flight", new osgGA::FlightManipulator() );
        //keyswitchManipulator->addMatrixManipulator( '3', "Drive",
        //    new osgGA::DriveManipulator() );

        initialize();
        view->addEventHandler(this);
        view->addEventHandler(keyswitchManipulator.get());
        view->addEventHandler(new osgViewer::StatsHandler);
      }
      view->setSceneData(scene);

      if(!isRTTWidget) graphicsCamera->setKeyswitchManipulator(keyswitchManipulator);
      graphicsCamera->changeCameraTypeToPerspective();
    }

    void GraphicsWindow::createContext(void* parent,
                                       GraphicsWindow* shared, int g_width, int g_height) {
      (void)parent;
      (void)g_width;
      (void)g_height;

      osg::ref_ptr<osg::Camera> osgCamera;

      if(!isRTTWidget) {
        osg::DisplaySettings* ds = osg::DisplaySettings::instance();
        osg::DisplaySettings* ds2 = new osg::DisplaySettings(*ds);
        osg::ref_ptr<osg::GraphicsContext::Traits> traits;
        traits = new osg::GraphicsContext::Traits;

        traits->readDISPLAY();
        if (traits->displayNum < 0)
          traits->displayNum = 0;
        traits->x = widgetX;
        traits->y = widgetY;
        traits->width = widgetWidth;
        traits->height = widgetHeight;
        traits->windowDecoration = true;
        traits->doubleBuffer = true;
        traits->alpha = ds->getMinimumNumAlphaBits();
        traits->stencil = ds->getMinimumNumStencilBits();
        traits->sampleBuffers = ds->getMultiSamples();
        traits->samples = ds->getNumMultiSamples();
        traits->vsync = false;
        if (shared) {
          traits->sharedContext = shared->getGraphicsWindow();
        } else {
          traits->sharedContext = 0;
        }

        osg::ref_ptr<osg::GraphicsContext> gc = createWidgetContext(parent, traits);

        graphicsWindow = dynamic_cast<osgViewer::GraphicsWindow*>(gc.get());
        osgCamera = new osg::Camera();
        osgCamera->setGraphicsContext(gc.get());
        osgCamera->setViewport(0, 0, widgetWidth, widgetHeight);
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        osgCamera->setDrawBuffer(buffer);
        osgCamera->setReadBuffer(buffer);
        osgCamera->setDisplaySettings(ds2);
        osgCamera->setCullMask(CULL_LAYER);
        view->setCamera(osgCamera.get());

        osg::Image* grabImage = new osg::Image();
        postDrawCallback = new PostDrawCallback(grabImage);
        postDrawCallback->setSize(widgetWidth, widgetHeight);
        postDrawCallback->setGrab(false);
        //osgCamera->setFinalDrawCallback(postDrawCallback);
      }
      else { // hasRTTWidget == true
        osgCamera = new osg::Camera();
        if(!shared) {
          osg::DisplaySettings* ds = osg::DisplaySettings::instance();
          osg::DisplaySettings* ds2 = new osg::DisplaySettings(*ds);
          (void)ds2;
          osg::ref_ptr<osg::GraphicsContext::Traits> traits;
          traits = new osg::GraphicsContext::Traits;
          traits->alpha = ds->getMinimumNumAlphaBits();
          traits->stencil = ds->getMinimumNumStencilBits();
          traits->windowDecoration = true;
          traits->sampleBuffers = ds->getMultiSamples();
          traits->samples = ds->getNumMultiSamples();
          traits->x = 0;
          traits->y = 0;
          traits->width = widgetWidth;
          traits->height = widgetHeight;
          traits->doubleBuffer = false;
          //traits->pbuffer = true;
          traits->windowDecoration = false;

          osg::ref_ptr<osg::GraphicsContext> gc;
          gc = osg::GraphicsContext::createGraphicsContext(traits.get());
          osgCamera->setGraphicsContext(gc.get());
        }
        else {
          osgCamera->setGraphicsContext(shared->getGraphicsWindow());
        }

        osg::DisplaySettings* ds = osg::DisplaySettings::instance();
        osg::DisplaySettings* ds2 = new osg::DisplaySettings(*ds);
        osgCamera->setDisplaySettings(ds2);
        osgCamera->setViewport(0, 0, widgetWidth, widgetHeight);
        view->setCamera(osgCamera.get());

        osgCamera->setRenderOrder(osg::Camera::PRE_RENDER);
        osgCamera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        osgCamera->setAllowEventFocus(false);
        osgCamera->setCullMask(CULL_LAYER);
        rttTexture = new osg::Texture2D();
        rttTexture->setResizeNonPowerOfTwoHint(false);
        rttTexture->setDataVariance(osg::Object::DYNAMIC);
        rttTexture->setTextureSize(widgetWidth, widgetHeight);
        rttTexture->setInternalFormat(GL_RGBA);
        rttTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        rttTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        rttTexture->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);
        rttTexture->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);



        rttImage = new osg::Image();
        rttImage->allocateImage(widgetWidth, widgetHeight,
                                1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV);
        osgCamera->attach(osg::Camera::COLOR_BUFFER, rttImage.get());
        rttTexture->setImage(rttImage);

        // depth component
        rttDepthTexture = new osg::Texture2D();
        rttDepthTexture->setResizeNonPowerOfTwoHint(false);
        rttDepthTexture->setDataVariance(osg::Object::DYNAMIC);
        rttDepthTexture->setTextureSize(widgetWidth, widgetHeight);
        rttDepthTexture->setSourceType(GL_UNSIGNED_INT);
        rttDepthTexture->setSourceFormat(GL_DEPTH_COMPONENT);
        rttDepthTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        rttDepthTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        rttDepthTexture->setFilter(osg::Texture2D::MIN_FILTER,
                                   osg::Texture2D::LINEAR);
        rttDepthTexture->setFilter(osg::Texture2D::MAG_FILTER,
                                   osg::Texture2D::LINEAR);
        rttDepthImage = new osg::Image();
        rttDepthImage->allocateImage(widgetWidth, widgetHeight,
                                     1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);

        osgCamera->attach(osg::Camera::DEPTH_BUFFER, rttDepthImage.get());
        
        std::fill(rttDepthImage->data(), rttDepthImage->data() + widgetWidth * widgetHeight * sizeof(GLuint), 0);
        
        rttDepthTexture->setImage(rttDepthImage);


      }
      graphicsCamera = new GraphicsCamera(osgCamera, widgetWidth, widgetHeight);
    }

    unsigned long GraphicsWindow::getID(void) {
      return widgetID;
    }

    osgViewer::View* GraphicsWindow::getView() {
      return view;
    }

    void GraphicsWindow::updateView(void) {
      // this hack solves temporary the initial window scaling problem
      static int initialResizeCount = 1;
      if(initialResizeCount > 0) {
        applyResize();
        --initialResizeCount;
      }

      graphicsCamera->update();
    }

    osgViewer::GraphicsWindow* GraphicsWindow::getGraphicsWindow() {
      return graphicsWindow.get();
    }
    const osgViewer::GraphicsWindow* GraphicsWindow::getGraphicsWindow() const {
      return graphicsWindow.get();
    }

    osg::ref_ptr<osg::Camera> GraphicsWindow::getMainCamera(){
      return graphicsCamera->getOSGCamera();
    }

    Vector GraphicsWindow::getMousePos(){
      return Vector(mouseX, mouseY, 0.0);
    }

    void GraphicsWindow::setFullscreen(bool val, int display) {
      if (!isFullscreen && val) {
        osg::GraphicsContext::WindowingSystemInterface *wsi =
          osg::GraphicsContext::getWindowingSystemInterface();
        unsigned int screenWidth = 1024;
        unsigned int screenHeight = 768;

        if (wsi != NULL) {
          wsi->getScreenResolution(*(graphicsWindow->getTraits()), screenWidth, screenHeight);
        }

        setWidgetFullscreen(true);
        isFullscreen = true;
        graphicsWindow->useCursor(false);
      } else if(isFullscreen && !val) {
        setWidgetFullscreen(false);
        isFullscreen = false;
        graphicsWindow->useCursor(true);
      }
    }

    void GraphicsWindow::setGraphicsEventHandler(GraphicsEventInterface* graphicsEventHandler) {
      this->graphicsEventHandler.push_back(graphicsEventHandler);
    }

    void GraphicsWindow::addGraphicsEventHandler(GraphicsEventInterface* graphicsEventHandler) {
      this->graphicsEventHandler.push_back(graphicsEventHandler);
    }

    GraphicsCameraInterface* GraphicsWindow::getCameraInterface(void) const {
      assert(graphicsCamera);
      return dynamic_cast<GraphicsCameraInterface*>(graphicsCamera);
    }


//    void GraphicsWindow::setHUD(HUD* theHUD) {
//      myHUD = theHUD;
//      theHUD->resize(widgetWidth, widgetHeight);
//      theHUD->setCullMask(CULL_LAYER);
//      theHUD->getCamera()->setFinalDrawCallback(postDrawCallback);
//      //view->addSlave(theHUD->getCamera(), false);
//      view->getCamera()->addChild( theHUD->getCamera() );
//    }
//
//    void GraphicsWindow::addHUDElement(HUDElement* elem) {
//      if(myHUD) myHUD->addHUDElement(elem);
//    }
//
//    void GraphicsWindow::removeHUDElement(HUDElement* elem) {
//      if(myHUD) myHUD->removeHUDElement(elem);
//    }
//
//    void GraphicsWindow::switchHudElemtVis(int num_element) {
//      if(myHUD) myHUD->switchElementVis(num_element);
//    }


    osg::Texture2D* GraphicsWindow::getRTTTexture(void) {
      return rttTexture.get();
    }

    osg::Texture2D* GraphicsWindow::getRTTDepthTexture(void) {
      return rttDepthTexture.get();
    }

    void GraphicsWindow::clearSelectionVectors() {
      pickedObjects.clear();
    }

    void GraphicsWindow::setGrabFrames(bool grab) {
      if(!isRTTWidget) postDrawCallback->setGrab(grab);
    }

    void GraphicsWindow::setSaveFrames(bool grab) {
      if(!isRTTWidget) postDrawCallback->setSaveGrab(grab);
    }

    std::vector<osg::Node*> GraphicsWindow::getPickedObjects() {
      return pickedObjects;
    }

    void GraphicsWindow::setClearColor(Color color){
      graphicsCamera->getOSGCamera()->setClearColor(color);
    }

    void GraphicsWindow::grabFocus() {
      getGraphicsWindow()->grabFocus();
    }

    void GraphicsWindow::getImageData(char* buffer, int& width, int& height)
    {
      if(isRTTWidget) {
        osg::Image *image = rttImage;
        width = image->s();
        height = image->t();
        memcpy(buffer, image->data(), width*height*4);
      }
      else
      {
        //slow but works...
        void *data;
        postDrawCallback->getImageData(&data, width, height);
        memcpy(buffer, data, width*height*4);
        free(data);
      }
    }

    void GraphicsWindow::getImageData(void **data, int &width, int &height) {
      if(isRTTWidget) {
        width = rttImage->s();
        height = rttImage->t();
        *data = malloc(width*height*4);
        getImageData((char *) *data, width, height);
      }
      else {
        postDrawCallback->getImageData(data, width, height);
      }
    }

    void GraphicsWindow::getRTTDepthData(float* buffer, int& width, int& height)
    {
      if(isRTTWidget) {
        GLuint* data2 = (GLuint *)rttDepthImage->data();
        width = rttDepthImage->s();
        height = rttDepthImage->t();

        double fovy, aspectRatio, Zn, Zf;
        graphicsCamera->getOSGCamera()->getProjectionMatrixAsPerspective( fovy, aspectRatio, Zn, Zf );
        int d = 0;
        for(int i=height-1; i>=0; --i) {
          for(int k=0; k<width; ++k) {
              GLuint di = data2[i*width+k];
              
            const float dv = ((float) di) / std::numeric_limits< GLuint >::max() ;
            // 1.0 is the max depth in the depth buffer, and
            // is represented as a nan in the distance image
            if( dv >= 1.0 )
              buffer[d++] = std::numeric_limits<float>::quiet_NaN();
            else
              buffer[d++] = Zn*Zf/(Zf-dv*(Zf-Zn));
          }
        }
      } else {
        throw std::runtime_error("Depth image not supported on non RTT Widges");
      }
    }

    void GraphicsWindow::getRTTDepthData(float **data, int &width, int &height) {
      if(isRTTWidget) {
        width = rttDepthImage->s();
        height = rttDepthImage->t();
        *data = (float*)malloc(width*height*sizeof(float));
        getRTTDepthData(data, width, height);
      } else {
        throw std::runtime_error("Depth image not supported on non RTT Widges");
      }
    }

    bool GraphicsWindow::handle(
                                const osgGA::GUIEventAdapter& ea,
                                osgGA::GUIActionAdapter& aa)
    {
      // remember position for mouse/camera interaction

      /*
      for(unsigned int i=0; i<graphicsEventHandler.size(); ++i) {
        graphicsEventHandler[i]->mouseMove(ea.getX(), ea.getY());
      }
      */

      // wrap events to class methods
      switch (ea.getEventType()) {
      case osgGA::GUIEventAdapter::KEYDOWN :
        sendKeyDownEvent(ea);
        return handleKeyDownEvent(ea);
      case osgGA::GUIEventAdapter::KEYUP :
        if (graphicsEventHandler.size() > 0) {
          graphicsEventHandler[0]->emitSetAppActive(widgetID);
        }
        sendKeyUpEvent(ea);
        return handleKeyUpEvent(ea);
      case osgGA::GUIEventAdapter::PUSH :
        return handlePushEvent(ea);
      case osgGA::GUIEventAdapter::MOVE :
        return handleMoveEvent(ea);
      case osgGA::GUIEventAdapter::DRAG :
        return handleDragEvent(ea);
      case osgGA::GUIEventAdapter::SCROLL:
        return handleScrollEvent(ea);
      case osgGA::GUIEventAdapter::RELEASE :
        return handleReleaseEvent(ea, aa);
      case osgGA::GUIEventAdapter::RESIZE :
        // the view does not receive release events for CTRL+D when the view is (un)docked
        // setting an empty event queue fixes his problem.
        view->setEventQueue(new osgGA::EventQueue);
        return handleResizeEvent(ea);
      case osgGA::GUIEventAdapter::FRAME :
        return false;
      case osgGA::GUIEventAdapter::QUIT_APPLICATION:
        if (graphicsEventHandler.size() > 0)
          graphicsEventHandler[0]->emitQuitEvent(widgetID);
        return true;
      case osgGA::GUIEventAdapter::CLOSE_WINDOW:
        return false;

      default:
        return false;
      }
    }

    void GraphicsWindow::applyResize() {
      postDrawCallback->setSize(widgetWidth, widgetHeight);
      graphicsCamera->setViewport(0, 0, widgetWidth, widgetHeight);
      graphicsCamera->changeCameraTypeToPerspective();
//      if (hudCamera) hudCamera->setViewport(0, 0, widgetWidth, widgetHeight);
//      if (myHUD) myHUD->resize(widgetWidth, widgetHeight);

      if(graphicsEventHandler.size() > 0) {
        graphicsEventHandler[0]->emitGeometryChange(widgetID,
                                                    widgetX, widgetY,
                                                    widgetWidth, widgetHeight);
      }
    }

    bool GraphicsWindow::handleResizeEvent(const osgGA::GUIEventAdapter& ea) {
      widgetWidth = ea.getWindowWidth();
      widgetHeight = ea.getWindowHeight();
      widgetX = ea.getWindowX();
      widgetY = ea.getWindowY();
      applyResize();
      return true;
    }

    bool GraphicsWindow::handleReleaseEvent(const osgGA::GUIEventAdapter& ea,
                                            osgGA::GUIActionAdapter& aa) {
      // *** Picking ***

      for(unsigned int i=0; i<graphicsEventHandler.size(); ++i) {
        graphicsEventHandler[i]->mouseRelease(ea.getX(), ea.getY(),
                                              ea.getButtonMask());
      }


      if(!isMouseMoving) {
        for(unsigned int i=0; i<graphicsEventHandler.size(); ++i) {
          graphicsEventHandler[i]->emitPickEvent(ea.getX(), ea.getY());
        }
      }

      isMouseButtonDown = false;
      isMouseMoving = false;


      if(pickmode == DISABLED) return false;

      osgViewer::View* view = dynamic_cast<osgViewer::View*>(&aa);
      if(!view) {
        cerr << "View cast failed!" << endl;
        return false;
      }

      // Mouse didn't move
      if(mouseX==(int)ea.getX() || mouseY==(int)ea.getY()) {
        if(pick(mouseX, mouseY)) {
          if(graphicsEventHandler.size() > 0) {
            graphicsEventHandler[0]->emitNodeSelectionChange(widgetID, (int)this->pickmode);
            return false;
          }
          else if(graphicsEventHandler.size() == 0) {
            cerr << "Error: object pick processing not possible : ";
            cerr << "no event handler found." << endl;
            return false;
          }
        }
      }

      return false;
    }

    bool GraphicsWindow::handleKeyDownEvent(const osgGA::GUIEventAdapter &ea) {
      switch (ea.getKey()) {
      case 'f' :
        return true;
      case osgGA::GUIEventAdapter::KEY_Escape :
        return true;
      case osgGA::GUIEventAdapter::KEY_Up:
        graphicsCamera->move(true, GraphicsCamera::FORWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Down:
        graphicsCamera->move(true, GraphicsCamera::BACKWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Left:
        graphicsCamera->move(true, GraphicsCamera::LEFT);
        return true;
      case osgGA::GUIEventAdapter::KEY_Right :
        graphicsCamera->move(true, GraphicsCamera::RIGHT);
        return true;
      default:
        return false;
      }
    }

    bool GraphicsWindow::handleKeyUpEvent(const osgGA::GUIEventAdapter &ea) {
      int key = ea.getKey();

      switch (key) {
      case osgGA::GUIEventAdapter::KEY_Escape :
        return true;
        break;
      case 'f' :
        setFullscreen(!isFullscreen);
        break;
//      case 'h' :
//        if (!isHUDShown) {
//          isHUDShown = 1;
//          if (myHUD) myHUD->setCullMask(CULL_LAYER);
//        }
//        else {
//          isHUDShown = 0;
//          if (myHUD) myHUD->setCullMask(0x0);
//        }
//        break;
      case osgGA::GUIEventAdapter::KEY_Up:
        graphicsCamera->move(false, GraphicsCamera::FORWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Down:
        graphicsCamera->move(false, GraphicsCamera::BACKWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Left:
        graphicsCamera->move(false, GraphicsCamera::LEFT);
        return true;
      case osgGA::GUIEventAdapter::KEY_Right :
        graphicsCamera->move(false, GraphicsCamera::RIGHT);
        return true;
      case '1' :
      case '2' :
      case '3' :
      case '4' :
      case '5' :
      case '6' :
      case '7' :
      case '8' :
      case '9' :
//        if (myHUD) myHUD->switchCullElement(key);
        break;
      case '.' :
        graphicsCamera->toggleStereoMode();
        break;
      case '+' :
        cameraEyeSeparation += 0.02;
        graphicsCamera->setEyeSep(cameraEyeSeparation);
        graphicsCamera->getOSGCamera()->getDisplaySettings()->setEyeSeparation(cameraEyeSeparation/10.0);
        break;
      case '-' :
        cameraEyeSeparation -= 0.02;
        graphicsCamera->setEyeSep(cameraEyeSeparation);
        graphicsCamera->getOSGCamera()->getDisplaySettings()->setEyeSeparation(cameraEyeSeparation/10.0);
        break;
      case '#' :
        isStereoDisplay = !isStereoDisplay;
        graphicsCamera->getOSGCamera()->getDisplaySettings()->setStereo(isStereoDisplay);
        break;
      case osgGA::GUIEventAdapter::KEY_Control_L :
      case osgGA::GUIEventAdapter::KEY_Control_R :
        this->pickmode = DISABLED;
        break;
      default:
        return false;
      } // switch

      return false;
    }

    bool GraphicsWindow::handlePushEvent(const osgGA::GUIEventAdapter& ea) {

      mouseX = ea.getX();
      mouseY = ea.getY();
      unsigned int modKey = ea.getModKeyMask();

      for(unsigned int i=0; i<graphicsEventHandler.size(); ++i) {
        graphicsEventHandler[i]->mousePress(ea.getX(), ea.getY(),
                                            ea.getButtonMask());
      }

      // we already are in camera move mode and therefore
      // do not want to enter pick mode
      if(isMouseButtonDown == true) {
        return false;
      }
      // we are not in camera move mode and enter pick mode
      else if(modKey & osgGA::GUIEventAdapter::MODKEY_CTRL) {
        if(this->pickmode == DISABLED)
          this->pickmode = STANDARD;
      }
      // we are not in camera move mode and also do not want to activate pick mode
      else {
        graphicsCamera->eventStartPos((int)ea.getX(), (int)ea.getY());
      }
      isMouseButtonDown = true;
      return false;
    }

    bool GraphicsWindow::handleDragEvent(const osgGA::GUIEventAdapter &ea) {
      for(unsigned int i=0; i<graphicsEventHandler.size(); ++i) {
        graphicsEventHandler[i]->mouseMove(ea.getX(), ea.getY());
      }

      if (isMouseButtonDown && pickmode == DISABLED) {
        graphicsCamera->mouseDrag(ea.getButtonMask(),
                                  (int)ea.getX(), (int)ea.getY());
        isMouseMoving = true;
      }
      return false;
    }

    bool GraphicsWindow::handleMoveEvent(const osgGA::GUIEventAdapter &ea) {
      for(unsigned int i=0; i<graphicsEventHandler.size(); ++i) {
        graphicsEventHandler[i]->mouseMove(ea.getX(), ea.getY());
      }
      return false;
    }

    bool GraphicsWindow::handleScrollEvent(const osgGA::GUIEventAdapter& ea)
    {
      if(ea.getScrollingMotion()==osgGA::GUIEventAdapter::SCROLL_UP){
        graphicsCamera->zoom(1);
      }else if(ea.getScrollingMotion()==osgGA::GUIEventAdapter::SCROLL_DOWN){
        graphicsCamera->zoom(-1);
      }
      return false;
    }

    void GraphicsWindow::sendKeyUpEvent(const osgGA::GUIEventAdapter &ea) {
      if (graphicsEventHandler.size() > 0) {
        int key = ea.getKey();
        unsigned int modKey = ea.getModKeyMask();
        unsigned int mod = 0;

        if (1 <= key && key <= 26) {
          key += 64;
        } else if (97 <= key && key <= 122) {
          key -= 32;
        } else if (key >= 0xFF00) {
          translateKey(key, mod);
        }

        if (modKey & osgGA::GUIEventAdapter::MODKEY_SHIFT) {
          mod |= GuiEventInterface::ShiftModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_CTRL) {
          mod |= GuiEventInterface::ControlModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_ALT) {
          mod |= GuiEventInterface::AltModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_META) {
          mod |= GuiEventInterface::MetaModifier;
        }
        std::vector<GraphicsEventInterface *>::iterator it;
        for(it=graphicsEventHandler.begin(); it!=graphicsEventHandler.end();
            ++it)
          (*it)->emitKeyUpEvent(key, mod, widgetID);
      }
    }

    void GraphicsWindow::sendKeyDownEvent(const osgGA::GUIEventAdapter &ea) {
      if (graphicsEventHandler.size() > 0) {
        int key = ea.getKey();
        unsigned int modKey = ea.getModKeyMask();
        unsigned int mod = 0;

        if (1 <= key && key <= 26) {
          key += 64;
        } else if (97 <= key && key <= 122) {
          key -= 32;
        } else if (key >= 0xFF00) {
          translateKey(key, mod);
        }

        if (modKey & osgGA::GUIEventAdapter::MODKEY_SHIFT) {
          mod |= GuiEventInterface::ShiftModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_CTRL) {
          mod |= GuiEventInterface::ControlModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_ALT) {
          mod |= GuiEventInterface::AltModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_META) {
          mod |= GuiEventInterface::MetaModifier;
        }
        std::vector<GraphicsEventInterface *>::iterator it;
        for(it=graphicsEventHandler.begin(); it!=graphicsEventHandler.end();
            ++it)
          (*it)->emitKeyDownEvent(key, mod, widgetID);
      }
    }

    void GraphicsWindow::translateKey(int &key, unsigned int &mod) {
      switch (key) {
      case osgGA::GUIEventAdapter::KEY_BackSpace :
        key = GuiEventInterface::Key_Backspace;
        break;
      case osgGA::GUIEventAdapter::KEY_Tab :
        key = GuiEventInterface::Key_Tab;
        break;
      case osgGA::GUIEventAdapter::KEY_Return :
        key = GuiEventInterface::Key_Return;
        break;
      case osgGA::GUIEventAdapter::KEY_Pause :
        key = GuiEventInterface::Key_Pause;
        break;
      case osgGA::GUIEventAdapter::KEY_Scroll_Lock :
        key = GuiEventInterface::Key_ScrollLock;
        break;
      case osgGA::GUIEventAdapter::KEY_Escape :
        key = GuiEventInterface::Key_Escape;
        break;
      case osgGA::GUIEventAdapter::KEY_Delete :
        key = GuiEventInterface::Key_Delete;
        break;
      case osgGA::GUIEventAdapter::KEY_Home :
        key = GuiEventInterface::Key_Home;
        break;
      case osgGA::GUIEventAdapter::KEY_Left :
        key = GuiEventInterface::Key_Left;
        break;
      case osgGA::GUIEventAdapter::KEY_Up :
        key = GuiEventInterface::Key_Up;
        break;
      case osgGA::GUIEventAdapter::KEY_Right :
        key = GuiEventInterface::Key_Right;
        break;
      case osgGA::GUIEventAdapter::KEY_Down :
        key = GuiEventInterface::Key_Down;
        break;
      case osgGA::GUIEventAdapter::KEY_Page_Up :
        key = GuiEventInterface::Key_PageUp;
        break;
      case osgGA::GUIEventAdapter::KEY_Page_Down :
        key = GuiEventInterface::Key_PageDown;
        break;
      case osgGA::GUIEventAdapter::KEY_End :
        key = GuiEventInterface::Key_End;
        break;
      case osgGA::GUIEventAdapter::KEY_Print :
        key = GuiEventInterface::Key_Print;
        break;
      case osgGA::GUIEventAdapter::KEY_Insert :
        key = GuiEventInterface::Key_Insert;
        break;
      case osgGA::GUIEventAdapter::KEY_Num_Lock :
        key = GuiEventInterface::Key_NumLock;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Space :
        key = GuiEventInterface::Key_Space;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Tab :
        key = GuiEventInterface::Key_Tab;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Enter :
        key = GuiEventInterface::Key_Enter;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F1 :
        key = GuiEventInterface::Key_F1;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F2 :
        key = GuiEventInterface::Key_F2;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F3 :
        key = GuiEventInterface::Key_F3;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F4 :
        key = GuiEventInterface::Key_F4;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Home :
        key = GuiEventInterface::Key_Home;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Left :
        key = GuiEventInterface::Key_Left;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Up :
        key = GuiEventInterface::Key_Up;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Right :
        key = GuiEventInterface::Key_Right;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Down :
        key = GuiEventInterface::Key_Down;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Page_Up :
        key = GuiEventInterface::Key_PageUp;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Page_Down :
        key = GuiEventInterface::Key_PageDown;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_End :
        key = GuiEventInterface::Key_End;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Insert :
        key = GuiEventInterface::Key_Insert;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Delete :
        key = GuiEventInterface::Key_Delete;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Equal :
        key = GuiEventInterface::Key_Equal;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Multiply :
        key = GuiEventInterface::Key_multiply;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Add :
        key = GuiEventInterface::Key_Plus;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Subtract :
        key = GuiEventInterface::Key_Minus;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Divide :
        key = GuiEventInterface::Key_Slash;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_0 :
        key = GuiEventInterface::Key_0;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_1 :
        key = GuiEventInterface::Key_1;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_2 :
        key = GuiEventInterface::Key_2;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_3 :
        key = GuiEventInterface::Key_3;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_4 :
        key = GuiEventInterface::Key_4;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_5 :
        key = GuiEventInterface::Key_5;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_6 :
        key = GuiEventInterface::Key_6;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_7 :
        key = GuiEventInterface::Key_7;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_8 :
        key = GuiEventInterface::Key_8;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_9 :
        key = GuiEventInterface::Key_9;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_F1 :
        key = GuiEventInterface::Key_F1;
        break;
      case osgGA::GUIEventAdapter::KEY_F2 :
        key = GuiEventInterface::Key_F2;
        break;
      case osgGA::GUIEventAdapter::KEY_F3 :
        key = GuiEventInterface::Key_F3;
        break;
      case osgGA::GUIEventAdapter::KEY_F4 :
        key = GuiEventInterface::Key_F4;
        break;
      case osgGA::GUIEventAdapter::KEY_F5 :
        key = GuiEventInterface::Key_F5;
        break;
      case osgGA::GUIEventAdapter::KEY_F6 :
        key = GuiEventInterface::Key_F6;
        break;
      case osgGA::GUIEventAdapter::KEY_F7 :
        key = GuiEventInterface::Key_F7;
        break;
      case osgGA::GUIEventAdapter::KEY_F8 :
        key = GuiEventInterface::Key_F8;
        break;
      case osgGA::GUIEventAdapter::KEY_F9 :
        key = GuiEventInterface::Key_F9;
        break;
      case osgGA::GUIEventAdapter::KEY_F10 :
        key = GuiEventInterface::Key_F10;
        break;
      case osgGA::GUIEventAdapter::KEY_F11 :
        key = GuiEventInterface::Key_F11;
        break;
      case osgGA::GUIEventAdapter::KEY_F12 :
        key = GuiEventInterface::Key_F12;
        break;
      }
    }

    bool GraphicsWindow::pick(const double x, const double y) {
      osgUtil::LineSegmentIntersector::Intersections intersections;
      osgUtil::LineSegmentIntersector::Intersections::iterator hitr;
      osg::PositionAttitudeTransform* posTransform;
      osg::Transform* transform;

#ifdef __linux__
      if(!view->computeIntersections(x, -y, intersections))
#else
        if(!view->computeIntersections(x, y, intersections))
#endif
          return false;

      // *** for each intersection we found ***
      for(hitr=intersections.begin(); hitr!=intersections.end(); ++hitr) {
        // choose foremost selection in FoV
        if(!(hitr==intersections.begin()) || !(!hitr->nodePath.empty()))
          continue;

        osg::NodePath nodePath = hitr->nodePath;
        unsigned int i = nodePath.size();
        while (i--) {
          transform = nodePath[i]->asTransform();
          if(!transform) continue;

          posTransform = transform->asPositionAttitudeTransform();
          if(!posTransform) continue;

          pickedObjects.push_back(posTransform);
          return true;
        }
      }

      return false;
    }

//    void GraphicsWindow::setHUDViewOffsets(double x1, double y1,
//                                           double x2, double y2) {
//      if(myHUD) {
//        myHUD->setViewOffsets(x1, x2, y1, y2);
//      }
//    }

  } // end of namespace graphics
} // end of namespace mars
