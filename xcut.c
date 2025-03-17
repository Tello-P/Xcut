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

// Global variables to store the clipboard text and its length.
char *clipboard_text = NULL;
size_t text_length = 0;

// Daemonize function to detach from the terminal and run in the background
void daemonize() {
	pid_t pid, sid;

	/* 1. Fork the parent process */
	pid = fork();
	if (pid < 0) {
		perror("First fork failed");
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		/* Parent process exits */
		exit(EXIT_SUCCESS);
	}

	/* 2. Create a new session to detach from the terminal */
	sid = setsid();
	if (sid < 0) {
		perror("setsid failed");
		exit(EXIT_FAILURE);
	}

	/* 3. Fork again to ensure the daemon cannot acquire a controlling terminal */
	pid = fork();
	if (pid < 0) {
		perror("Second fork failed");
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		/* First child exits */
		exit(EXIT_SUCCESS);
	}

	/* 4. Change the current working directory to root */
	if (chdir("/") < 0) {
		perror("chdir failed");
		exit(EXIT_FAILURE);
	}

	/* 5. Reset the file mode mask */
	umask(0);

	/* 6. Close standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Optionally, you can redirect these descriptors to /dev/null */
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
 * Function: my_strdup
 * -------------------
 * Creates a duplicate of the given string by allocating memory
 * and copying the content of the original string.
 *
 * Parameters:
 *   s - Pointer to the null-terminated string to duplicate.
 *
 * Returns:
 *   A pointer to the newly allocated duplicate of the string,
 *   or NULL if memory allocation fails.
 *
 * Example:
 *   char *original = "Hello, World!";
 *   char *copy = my_strdup(original);
 *   if (copy != NULL) {
 *       // Use the duplicated string...
 *       free(copy); // Don't forget to free the allocated memory.
 *   }
 */
char *my_strdup(const char *s)
{
	size_t len = strlen(s) + 1;  // +1 for the terminating null byte.
	char *copy = malloc(len);
	if (copy != NULL)
	{
		memcpy(copy, s, len);
	}
	return copy;
}

/*
 * Function: handle_selection_request
 * ------------------------------------
 * Handles selection requests from other applications.
 * When another application requests the clipboard content,
 * this function sends the text stored in 'clipboard_text'.
 *
 * Parameters:
 *   display - connection to the X server.
 *   event   - pointer to the X event that contains the selection request.
 *
 * Example:
 *   Suppose an application issues a paste command. It sends a 
 *   SelectionRequest event asking for the content in either XA_STRING
 *   or UTF8_STRING format. This function responds with the stored text.
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

	// Convert process to daemon
	daemonize();

	fseek(fp, 0, SEEK_END);
	text_length = ftell(fp);
	rewind(fp);

	clipboard_text = malloc(text_length + 1);
	if (!clipboard_text) {
		perror("Memory allocation failed");
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	fread(clipboard_text, 1, text_length, fp);
	clipboard_text[text_length] = '\0';
	fclose(fp);

	text_length = strlen(clipboard_text);

	Display *display = XOpenDisplay(NULL);
	if (display == NULL) {
		fprintf(stderr, "Unable to open display.\n");
		free(clipboard_text);
		exit(EXIT_FAILURE);
	}
	int screen = DefaultScreen(display);

	Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
			0, 0, 1, 1, 0,
			BlackPixel(display, screen),
			WhitePixel(display, screen));
	XSelectInput(display, window, PropertyChangeMask);

	Atom clipboard_atom = XInternAtom(display, "CLIPBOARD", False);
	XSetSelectionOwner(display, clipboard_atom, window, CurrentTime);
	if (XGetSelectionOwner(display, clipboard_atom) != window) {
		fprintf(stderr, "Failed to acquire clipboard ownership.\n");
		free(clipboard_text);
		XCloseDisplay(display);
		exit(EXIT_FAILURE);
	}

	printf("Text has been copied to the clipboard.\n");
	printf("The program will remain running to maintain the clipboard content.\n");
	printf("Press Ctrl+C to exit.\n");

	XEvent event;
	while (1) {
		XNextEvent(display, &event);
		switch (event.type) {
			case SelectionRequest:
				handle_selection_request(display, &event);
				break;
			case SelectionClear:
				printf("Clipboard ownership lost.\n");
				goto cleanup;
			default:
				break;
		}
	}

cleanup:
	free(clipboard_text);
	XDestroyWindow(display, window);
	XCloseDisplay(display);
	return 0;
}
