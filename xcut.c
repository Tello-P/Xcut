#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <errno.h>

// Globals for clipboard stuff
char *clipboard_text = NULL;
size_t text_length = 0;

// Turns everything into a daemon so user dont knows
void daemonize() {
	pid_t pid, sid;

	/* First fork: spawn a kid and let the parent die */
	pid = fork();
	if (pid < 0) {
		perror("First fork failed");
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		/* Parent’s gone */
		exit(EXIT_SUCCESS);
	}

	/* New session time—cuts ties with the terminal */
	sid = setsid();
	if (sid < 0) {
		perror("setsid failed");
		exit(EXIT_FAILURE);
	}

	/* Second fork: because one wasn’t enough, keeps it from grabbing a terminal */
	pid = fork();
	if (pid < 0) {
		perror("Second fork failed");
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		/* First kid’s done its job*/
		exit(EXIT_SUCCESS);
	}

	/* Move to root dir to not disturbe the peace */
	if (chdir("/") < 0) {
		perror("chdir failed");
		exit(EXIT_FAILURE);
	}

	/* Reset file permissions*/
	umask(0);

	/* Standard I/O doors shut */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Could send stuff to /dev/null, but its working now uncomment if you wish */
	/* int fd = open("/dev/null", O_RDWR);
	   if (fd != -1) {
	   dup2(fd, STDIN_FILENO);
	   dup2(fd, STDOUT_FILENO);
	   dup2(fd, STDERR_FILENO);
	   if (fd > 2) close(fd);
	   }
	   */
}

/* 
 * my_strdup: DIY string copier since strdup isn’t always around
 * Takes a string, makes a fresh copy, hands it back
 * Give it a string, get a duplicate or NULL if malloc hates you
 * Example: char *s = "something"; char *c = my_strdup(s);
 */
char *my_strdup(const char *s)
{
	size_t len = strlen(s) + 1;  // Yes that null byte too
	char *copy = malloc(len);
	if (copy != NULL)
	{
		memcpy(copy, s, len);
	}
	return copy;
}

/*
 * handle_selection_request: Deals with apps asking for clipboard goodies
 * When someone wants the text, this shoves it their way
 * Needs the X display and the event details to work
 * If you paste somewhere, this catches the request and delivers
 */
void handle_selection_request(Display *display, XEvent *event) {
	XSelectionRequestEvent *req = &event->xselectionrequest;
	XEvent response;
	memset(&response, 0, sizeof(response));

	response.xselection.type      = SelectionNotify;
	response.xselection.display   = req->display;
	response.xselection.requestor = req->requestor;
	response.xselection.selection = req->selection;
	response.xselection.target    = req->target;
	response.xselection.time      = req->time;
	response.xselection.property  = req->property;

	Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);

	if (req->target == XA_STRING || req->target == utf8_string) {
		XChangeProperty(display, req->requestor, req->property,
				req->target, 8, PropModeReplace,
				(unsigned char*)clipboard_text, text_length);
	} else if (req->target == XInternAtom(display, "TARGETS", False)) {
		Atom targets[3];
		int count = 0;
		targets[count++] = XA_STRING;
		targets[count++] = utf8_string;
		targets[count++] = XInternAtom(display, "TARGETS", False);
		XChangeProperty(display, req->requestor, req->property,
				XA_ATOM, 32, PropModeReplace,
				(unsigned char*)targets, count);
	} else {
		response.xselection.property = None;
	}

	XSendEvent(display, req->requestor, 0, 0, &response);
	XFlush(display);
}

int main(int argc, char *argv[]) {
	errno = 0;
	FILE *fp = NULL;

	// Just as usual
	if (argc > 1) {
		fp = fopen(argv[1], "r");
		if (!fp) {
			perror("Error opening file");
			exit(EXIT_FAILURE);
		}
	} else {
		errno = 2;
		perror("ERROR: No file provided");
		exit(EXIT_FAILURE);
	}

	// Daemon now
	daemonize();

	// Figure out how big the file is
	fseek(fp, 0, SEEK_END);
	text_length = ftell(fp);
	rewind(fp);

	// Grab some memory for the text (malloc) :)
	clipboard_text = malloc(text_length + 1);
	if (!clipboard_text) {
		perror("Memory allocation failed");
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	// Get the file contents
	fread(clipboard_text, 1, text_length, fp);
	clipboard_text[text_length] = '\0';	// So ist a finished text
	fclose(fp);

	text_length = strlen(clipboard_text); // Double-check the length
	
	/*
	 * X11
	 */
	Display *display = XOpenDisplay(NULL);
	if (display == NULL) {
		fprintf(stderr, "Unable to open display.\n");
		free(clipboard_text);
		exit(EXIT_FAILURE);
	}
	int screen = DefaultScreen(display);

	// Make a tiny window for X11 shenanigans (later will go out)
	Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
			0, 0, 1, 1, 0,
			BlackPixel(display, screen),
			WhitePixel(display, screen));
	XSelectInput(display, window, PropertyChangeMask);

	// Claim the clipboard
	Atom clipboard_atom = XInternAtom(display, "CLIPBOARD", False);
	XSetSelectionOwner(display, clipboard_atom, window, CurrentTime);
	if (XGetSelectionOwner(display, clipboard_atom) != window) {
		fprintf(stderr, "Failed to acquire clipboard ownership.\n");
		free(clipboard_text);
		XCloseDisplay(display);
		exit(EXIT_FAILURE);
	}

	// Let the user know (though they might not see this as a daemon, but you never know)
	printf("Text has been copied to the clipboard.\n");
	printf("The program will remain running to maintain the clipboard content.\n");
	printf("Press Ctrl+C to exit.\n");

	// Event loop, wait for clipboard action
	XEvent event;
	while (1) {
		XNextEvent(display, &event);
		switch (event.type) {
			case SelectionRequest:
				handle_selection_request(display, &event);
				break;
			case SelectionClear:
				printf("Clipboard ownership lost.\n");
				goto cleanup; // Exit if someone else takes over
			default:
				break;
		}
	}

cleanup:
	// Clean everything and close
	free(clipboard_text);
	XDestroyWindow(display, window);
	XCloseDisplay(display);
	return 0;
}

