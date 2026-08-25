on run
	set installScript to quoted form of (POSIX path of (path to resource "install.sh"))
	try
		set outcome to do shell script installScript with administrator privileges
		display dialog outcome buttons {"OK"} default button "OK" ¬
			with title "Installation Complete" with icon note
	on error errMsg number errNum
		if errNum is -128 then return
		display dialog "Installation failed." & return & return & errMsg ¬
			buttons {"OK"} default button "OK" with title "Installation Failed" with icon stop
	end try
end run
