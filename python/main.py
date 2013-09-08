
import curses
import wwiitt

def scaniprange(iprange):
	banner()
	workwindow.addstr(14,1,"  Scanning ip range " + iprange)
	ips = iprange.split(" ")
	wwiitt.ipscan(ips[0],ips[1],logwindow,workwindow,screen)

screen = curses.initscr()

# Create work windows and log window
(nlines,ncols) = screen.getmaxyx()
#print nlines,ncols
logwindow  = screen.subwin(nlines-1,ncols/2,0,ncols/2)
workwindow = screen.subwin(nlines-1,ncols/2,0,0)
#logpad = screen.subpad(nlines-1,ncols/2)

def banner():
	workwindow.clear()
	workwindow.addstr(1 ,1,"  __      __  __      __  ______  ______  ______")
	workwindow.addstr(2 ,1," /\ \  __/\ \/\ \  __/\ \/\__  _\/\__  _\/\__  _\ ")
	workwindow.addstr(3 ,1," \ \ \/\ \ \ \ \ \/\ \ \ \/_/\ \/\/_/\ \/\/_/\ \/ ")
	workwindow.addstr(4 ,1,"  \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \   \ \ \   \ \ \ ")
	workwindow.addstr(5 ,1,"   \ \ \_/ \_\ \ \ \_/ \_\ \ \_\ \__ \ \ \   \ \ \ ")
	workwindow.addstr(6 ,1,"    \ `\___x___/\ `\___x___/ /\_____\ \ \_\   \ \_\ ")
	workwindow.addstr(7 ,1,"     '\/__//__/  '\/__//__/  \/_____/  \/_/    \/_/ ")
	workwindow.addstr(9 ,1,"         World Wide Internet Takeover Tool")

# Main window
def mainwin():
	banner()
	workwindow.addstr(12,1,"    Select option:")
	workwindow.addstr(14,1,"     1. Port scan")
	workwindow.addstr(15,1,"     2. Virtualhost scan")
	workwindow.addstr(16,1,"     3. HTTP index crawler")

def portscanwin():
	banner()
	workwindow.addstr(14,1,"  Enter IP address range")
	
def updatelog():
	logwindow.border()

menu = 0
while True:
	if menu == 0:
		mainwin()
	elif menu == 1:
		portscanwin()
	else:
		pass
	
	updatelog()
	workwindow.border()
	workwindow.redrawwin()
	workwindow.refresh()
	
	#logpad.addstr(1,1,"uhuhuhu")
	#logpad.refresh(0,0,0,0,10,10)
	logwindow.redrawwin()
	logwindow.refresh()
	
	userinput = ""
	try:
		userinput = screen.getstr(nlines-1,0)
	except:
		break
	
	if menu == 0:
		menu = int(userinput)
	elif menu == 1:
		scaniprange(userinput)
		menu = 0
	userinput = ""

curses.endwin()

