# invoke SourceDir generated makefile for GUI.pem4f
GUI.pem4f: .libraries,GUI.pem4f
.libraries,GUI.pem4f: package/cfg/GUI_pem4f.xdl
	$(MAKE) -f H:\egh456_ws_Assignment\EGH456_Assignment/src/makefile.libs

clean::
	$(MAKE) -f H:\egh456_ws_Assignment\EGH456_Assignment/src/makefile.libs clean

