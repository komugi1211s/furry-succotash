package main
import (
	"os"
	"os/exec"
	"os/signal"
	"sync"
	"path"
	"log"
	"fmt"
	"time"
	"bufio"
	"syscall"
	"strings"
)

var runningCount int = 0

func findLastestModifiedTime(lookingPath string) (time.Time, error) {
	Status, err := os.Stat(lookingPath)

	if err != nil {
		return time.Unix(0, 0), err
	}

	if !Status.IsDir() {
		return Status.ModTime(), nil
	} else {
		files, err := os.ReadDir(lookingPath)
		if err != nil {
			return time.Unix(0, 0), err
		}

		newestTime := time.Unix(0, 0)

		for _, v := range files {
			newPath  := path.Join(lookingPath, v.Name())
			lastTime, err := findLastestModifiedTime(newPath)
			if err != nil {
				return time.Unix(0, 0), err
			}

			if lastTime.After(newestTime) {
				newestTime = lastTime
			}
		}
		return newestTime, nil
	}
}

func setupOutput(command *exec.Cmd, logger *log.Logger) error {
	stdoutPipe, err := command.StdoutPipe()
	if err != nil {
		return err
	}
	stderrPipe, err := command.StderrPipe()
	if err != nil {
		return err
	}

	stdout := bufio.NewScanner(stdoutPipe)
	stderr := bufio.NewScanner(stderrPipe)

	go func() {
		if e := stdout.Err(); e != nil {
			logger.Fatalln("Stdout sput out an error: ", e)
		}
		for stdout.Scan() {
			logger.Print("(Out) ", stdout.Text())
		}
	}()

	go func() {
		if e := stderr.Err(); e != nil {
			logger.Fatalln("Stderr sput out an error: ", e)
		}
		for stderr.Scan() {
			logger.Print("(Err) ", stderr.Text())
		}
	}()
	return nil
}

type Screen struct {
	mutex sync.Mutex
	watcherLine []string
	processLine []string
	commandLine []string
}

const SIZE_OF_TERM = 10

var screen = Screen {
	watcherLine: make([]string, 0, SIZE_OF_TERM),
	processLine: make([]string, 0, SIZE_OF_TERM),
}

func(it *Screen) Render() {
	fmt.Print("\033[H\033[2J\n")
	{
		fmt.Println("[WATCHER] ======== ======")
		lengthOfLine := len(it.watcherLine)
		for _, v := range it.watcherLine {
			fmt.Println("   ", v)
		}
		if lengthOfLine < SIZE_OF_TERM {
			for i := 0; i < (SIZE_OF_TERM - lengthOfLine); i++ {
				fmt.Print("\n")
			}
		}
		fmt.Println("======== ======== ========")
	}

	{
		fmt.Println("[PROCESS] ======== =======")
		lengthOfLine := len(it.processLine)
		for _, v := range it.processLine {
			fmt.Println("   ", v)
		}
		if lengthOfLine < SIZE_OF_TERM {
			for i := 0; i < (SIZE_OF_TERM - lengthOfLine); i++ {
				fmt.Print("\n")
			}
		}
		fmt.Println("======== ======== ========")
	}
}

func(it *Screen) watchLog(f string) {
	it.mutex.Lock()
	defer it.mutex.Unlock()
	it.watcherLine = append(it.watcherLine, fmt.Sprintf(":%d: %s", runningCount, f))
	if len(it.watcherLine) > SIZE_OF_TERM {
		it.watcherLine = it.watcherLine[1:]
	}
	it.Render()
}

func(it *Screen) procLog(f string) {
	it.mutex.Lock()
	defer it.mutex.Unlock()
	it.processLine = append(it.processLine, fmt.Sprintf(":%d: %s", runningCount, f))
	if len(it.processLine) > SIZE_OF_TERM {
		it.processLine = it.processLine[1:]
	}
	it.Render()
}

type Proc  struct {}
type Watch struct {}

func(it Proc)  Write(p []byte) (n int, err error) {
	if p[len(p)-1] == '\n' {
		p = p[:len(p)-1]
	}
	screen.procLog(string(p))
	return len(p), nil
}

func(it Watch) Write(p []byte) (n int, err error) {
	if p[len(p)-1] == '\n' {
		p = p[:len(p)-1]
	}
	screen.watchLog(string(p))
	return len(p), nil
}

var grandProcess *exec.Cmd
var processManipulationMutex sync.Mutex

func beginProcess(procLog *log.Logger, command []string) {
	processManipulationMutex.Lock()
	defer processManipulationMutex.Unlock()
	process := command[0]
	args    := command[1:]

	grandProcess = exec.Command(process, args...)
	grandProcess.SysProcAttr = &syscall.SysProcAttr{ Setpgid: true }
	setupOutput(grandProcess, procLog)

	runningCount += 1
	go grandProcess.Run()
}

// NOTE: Don't you dare lock the mutex inside this function --
// it will cause a deadlock.
func replaceProcess(procLog *log.Logger, command []string) {
	procLog.Println("Replacing a process.")
	closeProcess()
    beginProcess(procLog, command)
}

func closeProcess() {
	processManipulationMutex.Lock()
	defer processManipulationMutex.Unlock()
	if grandProcess != nil {
		syscall.Kill(-grandProcess.Process.Pid, syscall.SIGTERM)
	}
}

func main() {
	watcherLog := log.New(Watch{}, "", log.Default().Flags())
	processLog := log.New(Proc{},  "", log.Default().Flags())
	watcherLog.Println("Waiting for a setup.")

	args := os.Args[1:]

	lookingFolder  := ""
	processCommand := []string{}

	if len(args) == 0 {
		watcherLog.Fatalln("Usage: re_run.go -f `folder` -p `command`")
	}

	cursor := 0
	for cursor < len(args) {
		s := args[cursor]
		if s == "-f" {
			if lookingFolder == "" {
				cursor++
				lookingFolder = args[cursor]
			} else {
				watcherLog.Fatalln("Multiple folder specified: not supported yet.")
			}
		}

		if s == "-p" {
			cursor++
			for cursor < len(args) && !strings.HasPrefix(args[cursor], "-") {
				processCommand = append(processCommand, args[cursor])
				cursor++
			}
		}

		cursor++
	}

	watcherLog.Printf("Folder Specified: %s", lookingFolder)
	watcherLog.Printf("Command Specfied: %s", strings.Join(processCommand, " "))

	watcherLog.Println("Starting New Process...")

	LastModTime, err := findLastestModifiedTime(lookingFolder)
	if err != nil {
		screen.watchLog(fmt.Sprint("Failed to read latest modified time: ", err))
	}

	cmdLineChannel := make(chan string, 1)
	sigChannel     := make(chan os.Signal, 1)

	signal.Notify(sigChannel, os.Interrupt)

	go func() {
		for {
			select {
				case v := <-cmdLineChannel:
				{
					watcherLog.Println("Got Command:", v)
					switch v {
						case "exit":
						{
							closeProcess()
							watcherLog.Fatalln("Quitting.")
						}

						case "help":
						{
							watcherLog.Println("`exit` will quit the process.")
							watcherLog.Println("`restart` will restart the process.")
							watcherLog.Println("`help` will show this message.")
						}

						case "restart":
						{
							replaceProcess(processLog, processCommand)
							watcherLog.Println("Restarting process.")
						}

						case "set_process":
						{
							watcherLog.Println("Setting process to: ", v)
							processCommand = strings.Split(v, " ")
							replaceProcess(processLog, processCommand)
						}
					}
				}

				case s := <-sigChannel:
					closeProcess()
					watcherLog.Fatalln("Killing Processes: ", s)
			}
		}
	}()

	go func() {
		for {
			var result string
			read, err := fmt.Scanln(&result)
			if err != nil {
				watcherLog.Println("Failed to read commandline via scanf: ", err)
			}
			if read != 0 {
				cmdLineChannel <- result
			}
		}
	}()

	beginProcess(processLog, processCommand)
	watcherLog.Println("Began process.")

	for {
		if grandProcess != nil && grandProcess.ProcessState != nil {
			if grandProcess.ProcessState.ExitCode() != 0 {
				watcherLog.Printf("Process returned non-zero exit code: %d\n", grandProcess.ProcessState.ExitCode())
			}

			watcherLog.Println("Process Exited, watching modification so I can restart")
			grandProcess = nil
		}

		latestTime, err := findLastestModifiedTime(lookingFolder)
		if err != nil {
			closeProcess()
			watcherLog.Fatalln("Failed to get the latest modified time: ", err)
		}

		if latestTime.After(LastModTime) {
			watcherLog.Println("Restarting New Process...")
			replaceProcess(processLog, processCommand)
			LastModTime = latestTime
		}

		time.Sleep(time.Millisecond * 250)
	}
}
