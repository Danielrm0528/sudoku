#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <gtk-3.0/gtk/gtk.h>
#include <gtk/gtkx.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>

GtkWidget	*window;
GtkWidget	*fixedElement, *timeC, *timeLabel, *menuBox, *solveButton, *clearButton, *exitButton, *M1, *M2, *M3, *newButton, *saveAsButton, *saveButton, *loadButton, *disabledMenu;
GtkWidget	*grids[11];
GtkLabel *textResultLabel, *fileNameLabel, *fileName;
GtkBuilder	*builder; 
GTimer *stopwatch;

//hola

char *currentFileName = ""; //Save the path of the current file
int incompatibleFile = 0; //1 for error reading the file
bool stopProgram = FALSE; //1 for interruption when the program is finding the solution of the sudoku
gdouble seconds = 0;
bool solving = false;
bool newSudoku = true;
bool clear = true;

typedef struct {
    int num;
    GtkButton *button;
	bool pista;
} ButtonData;

ButtonData buttons[81];

//restore buttons for use
void activate_buttons(){
	gtk_widget_set_visible(clearButton,TRUE);
	gtk_widget_set_visible(saveButton,TRUE);
	gtk_widget_set_visible(loadButton,TRUE);
	gtk_widget_set_visible(saveAsButton,TRUE);
	gtk_widget_set_visible(solveButton,TRUE);
	gtk_widget_set_visible(newButton,TRUE);
	gtk_widget_set_visible(disabledMenu,FALSE); //no useful, just indicates the other buttons are disabled
}

//hide buttons while the sudoku solver is running
void hide_buttons(){
	gtk_widget_set_visible(clearButton,FALSE);
	gtk_widget_set_visible(saveButton,FALSE);
	gtk_widget_set_visible(loadButton,FALSE);
	gtk_widget_set_visible(saveAsButton,FALSE);
	gtk_widget_set_visible(solveButton,FALSE);
	gtk_widget_set_visible(newButton,FALSE);
	gtk_widget_set_visible(disabledMenu, TRUE); //show the "button" that indicates the rest of buttons are disabled
}

//converts time to printable string
void timer(gboolean counting) {
	int hours,minutes;
	gchar str[50];
	if(counting) {
		hours = seconds / 3600;
		seconds -= 3600 * hours;
		minutes = seconds / 60;
		seconds -= 60 * minutes;
		sprintf(str, "%02d:%02d:%.2f", hours, minutes, seconds);
	}
	gtk_label_set_text (GTK_LABEL (timeC), str);
}

//handles time counting loop
gboolean updateElapsedTime() {
    gulong microseconds;
	if(solving) {
		seconds = g_timer_elapsed(stopwatch,&microseconds);
		timer(TRUE);
		return TRUE;
	} else {
		g_timer_stop(stopwatch);
		return FALSE;
	}
}

//sudoku's button pressed
void on_b_clicked(GtkButton *button, gpointer Udata) {
	if(solving || !newSudoku) return;
	gtk_label_set_text(textResultLabel, "");
	ButtonData *data = (ButtonData*) Udata;
	int num = data->num;
    GtkButton *b = data->button;
	if(num >= 9) num = 0;
	else num++;
	char strNum[5];
	snprintf(strNum, sizeof(strNum),"%d",num);
	if(num != 0) gtk_button_set_label(button, strNum);
	else gtk_button_set_label(b, " ");
	data->num = num;
	if(data->num != 0) {
		data->pista = true; 
		//change button color
		GdkColor color;
        gdk_color_parse ("#FF0000", &color);
        gtk_widget_modify_fg (button, GTK_STATE_NORMAL, &color);
	}
	else {
		data->pista = false;
		GdkColor color;
        gdk_color_parse ("#000000", &color);
        gtk_widget_modify_fg (button, GTK_STATE_NORMAL, &color);
	}
}

void on_clear_clicked() {
	if(clear) gtk_label_set_text(textResultLabel, "");
	for(int i = 0;i<81;i++) {
		buttons[i].num = 0;
		buttons[i].pista = false;
		gtk_button_set_label(buttons[i].button, " ");
		GdkColor color;
        gdk_color_parse ("#000000", &color);
        gtk_widget_modify_fg (buttons[i].button, GTK_STATE_NORMAL, &color);
	}
	newSudoku = true;
	gtk_label_set_text(timeC, "00:00:00");
}

//indicates stop the program
void on_exit_clicked(){
	if(solving) {
		stopProgram = TRUE; 
		newSudoku = false;
	}
	solving = false;
}

int lastCell(ButtonData puzzle[], int cell) {
	for(int i = cell-1;i>=0;i--) {
		if(!puzzle[i].pista) return i;
	}
	return 0;
}

bool hasLastCell(ButtonData puzzle[], int cell) {
	for(int i = cell-1;i>=0;i--) {
		if(!puzzle[i].pista) return true;
	}
	return false;
}

bool hasEmptyCells(ButtonData puzzle[]) {
	for(int i = 0;i<81;i++) {
		if(puzzle[i].num == 0 && !puzzle[i].pista) return true;
	}
	return false;
}

int nextEmptyCell(ButtonData puzzle[], int cell) {
	for(int i = cell+1;i<81;i++) {
		if(puzzle[i].num == 0 && !puzzle[i].pista) return i;
	}
	return 81;
}

bool validate(ButtonData puzzle[], int cell) {
	int num = puzzle[cell].num;
	int row = cell/9;
	int col = cell%9;
	//check row validity
	for(int i = 0;i<9;i++) {
		if(row*9+i != cell && puzzle[row*9+i].num == num && puzzle[row*9+i].num != 0) return false;
	}
	//check column validity
	for(int i = 0;i<9;i++) {
		if(i*9+col != cell && puzzle[i*9+col].num == num && puzzle[i*9+col].num != 0) return false;
	}
	
	int subRow = (row/3) * 3;
	int subCol = (col/3) * 3;
	for(int i = 0;i<3;i++) {
		for(int j = 0;j<3;j++) {
			int idx = (subRow + i) * 9 + subCol + j;
			if(idx != cell && puzzle[idx].num == num) return false;
	 	}
	}
	return true;
}

//struct for sudoku parameters
typedef struct {
	int cell;
	ButtonData *buttons;
}SudokuSolver;

//solve with partial solutions
gboolean solveStep(gpointer *dataI) {
	SudokuSolver *data = (SudokuSolver *)dataI;
	int cell = data->cell;
	ButtonData *buttons = data->buttons;
	//check interruption (stop the program)
	if (stopProgram == TRUE) {
		stopProgram = FALSE;
		gtk_label_set_text(textResultLabel, "Solution stopped.");
		activate_buttons();
		//if a cell is still a 10, convert to a 0 if program stopped
		for(int i = 0;i<81;i++) {
			if(buttons[i].num > 9) buttons[i].num = 0;
		}	
		return FALSE; //break the loop
	}
	//if number is not valid then increment
	if(buttons[cell].num != 0 && !validate(buttons,cell) && buttons[cell].num <=9) {
		buttons[cell].num = buttons[cell].num + 1;
		//print en label
		char strNum[5];
		snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num );
		if(buttons[cell].num  != 0) gtk_button_set_label(buttons[cell].button, strNum);
		else gtk_button_set_label(buttons[cell].button, " ");
		return TRUE;
	}
	//if number is valid
	if(buttons[cell].num != 0 && buttons[cell].num <=9) {
		//if there are empty cells
		if(hasEmptyCells(buttons)){
			cell = nextEmptyCell(buttons,cell);
			data->cell = cell;
			buttons[cell].num = buttons[cell].num + 1;
			//print en label
			char strNum[5];
			snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num);
			gtk_button_set_label(buttons[cell].button, strNum);
			return TRUE;
		} else {
			gtk_label_set_text(textResultLabel, "Solution found successfully.");
			activate_buttons();
			solving = false;
			newSudoku = true;
			return FALSE; //solution finished
		}
	} else {
			buttons[cell].num = 0;
			gtk_button_set_label(buttons[cell].button, " ");
			if(hasLastCell(buttons,cell)) {
				cell = lastCell(buttons,cell);
				data->cell = cell;
				buttons[cell].num = buttons[cell].num + 1;
				//print en label
				char strNum[5];
				snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num);
				gtk_button_set_label(buttons[cell].button, strNum);
				return TRUE;
			} else {
				activate_buttons();
				gtk_label_set_text(textResultLabel, "The solution could not be found. Maybe some clues were entered incorrectly.");
				solving = false;
				newSudoku = true;
				return FALSE; //no solution
			}
		}
}

//solve without partial solutions (not used and not updated)
bool solve() {

	int cell = 0;
	while(buttons[cell].num != 0) {
		cell++;
	}
	//no empty cell, no solution
	if(cell == 81) {
		return true;
	}
	buttons[cell].num = 1;
	char strNum[5];
	//print en label
	snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num);
	gtk_button_set_label(buttons[cell].button, strNum);
	bool terminado = false;
	while(!terminado) {
		while(buttons[cell].num != 0 && !validate(buttons,cell) && buttons[cell].num <=9) {
			buttons[cell].num = buttons[cell].num + 1;
			char strNum[5];
			//print en label
			snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num );
			if(buttons[cell].num  != 0) gtk_button_set_label(buttons[cell].button, strNum);
			else gtk_button_set_label(buttons[cell].button, " ");
		}
		if(buttons[cell].num != 0 && buttons[cell].num <=9) {
			if(hasEmptyCells(buttons)){
				cell = nextEmptyCell(buttons,cell);
				buttons[cell].num = buttons[cell].num + 1;
				char strNum[5];
				//print en label
				snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num);
				gtk_button_set_label(buttons[cell].button, strNum);
			} else {
				terminado = true;
			}
		} else {
			buttons[cell].num = 0;
			gtk_button_set_label(buttons[cell].button, " ");
			if(hasLastCell(buttons,cell)) {
				cell = lastCell(buttons,cell);
				buttons[cell].num = buttons[cell].num + 1;
				char strNum[5];
				//print en label
				snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num);
				gtk_button_set_label(buttons[cell].button, strNum);
			} else {
				terminado = true;
			}
		}
		gtk_widget_queue_draw(GTK_WIDGET(buttons[cell].button));
	}
}

//instructions for solve button
void on_resolver_clicked(GtkButton *button) {
	solving = true;
	gtk_label_set_text(textResultLabel, "");
	//all cells that are not 0 are clues
	if (newSudoku){
		for(int i = 0;i<81;i++) {
			if(buttons[i].num != 0) {
				buttons[i].pista = true;
				GdkColor color;
				gdk_color_parse ("#FF0000", &color);
				gtk_widget_modify_fg (buttons[i].button, GTK_STATE_NORMAL, &color);
			}
		}
	}else g_timer_continue(stopwatch);
	hide_buttons();
	//solve(); return;
	int cell = 0;
	//changes to first empty cell
	while(buttons[cell].num != 0) {
		cell++;
	}
	//no empty cell, no solution
	if(cell == 81) {
		gtk_label_set_text(textResultLabel, "The solution could not be found. No empty cells.");
		activate_buttons();
		solving = false;
		newSudoku = true;
		return;
	}
	buttons[cell].num = 1;
	//print in label
	char strNum[5];
	snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num);
	gtk_button_set_label(buttons[cell].button, strNum);
	SudokuSolver *data = g_new(SudokuSolver, 1);
    if (data == NULL) {
        // Handle memory allocation failure
        return;
    }
	data->buttons = buttons;
	data->cell = cell;
	//start loop
	if(newSudoku) stopwatch = g_timer_new();
	g_timeout_add( 50, (GSourceFunc) updateElapsedTime, NULL);
	g_timeout_add(1, G_SOURCE_FUNC(solveStep), data);
}

//save function
void save(char *name){
	FILE *file = fopen((char*)name,"w");
	char esp = ' ';
    char enter = '\n';
	for (int cell=0;cell<81;cell++){
		int num = buttons[cell].num + 48;
		(char)num;
		putc(num, file);
		if((cell+1)%9==0) putc(enter, file);
		else putc(esp, file);
	}
	rewind(file);
	fclose(file);
}

//save (update) sudoku file
void on_saveButton_clicked(){
	if(currentFileName != ""){
		save(gtk_label_get_text(fileName));
		gtk_label_set_text(textResultLabel, "File saved/updated successfully in the project folder.");
	}else gtk_label_set_text(textResultLabel,"There's no file open to update.");
}

//creates a new sudoku
void on_new_clicked(){
	on_clear_clicked();
	gtk_label_set_text(fileName, "**No current file**");
	currentFileName = "";
}

//save sudoku to a file as ...
void on_saveAsButton_clicked(){
	gtk_label_set_text(textResultLabel, "");

	GtkWidget *dialog = gtk_file_chooser_dialog_new("Save file", (GtkWindow*) window, GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
	gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
	if(currentFileName == "") 
		gtk_file_chooser_set_current_name(chooser, "Untitled document");
	else{ 
		gtk_file_chooser_select_filename(chooser, gtk_label_get_text(fileName)); //La idea es que abra archivos en la carpeta que contine el file, pero no lo hace
		gtk_file_chooser_set_current_name(chooser, gtk_label_get_text(fileName));
	}
	gint res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT){
		char *filename = gtk_file_chooser_get_filename(chooser);
		currentFileName = filename;

		save(filename); //save file with sudoku information

		gtk_label_set_text(fileName, g_path_get_basename(currentFileName));
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}

//loadButton sudoku from file
void on_loadButton_clicked(){
	//create window and choose action(open)
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Open file", (GtkWindow *)window, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open",GTK_RESPONSE_ACCEPT, NULL);
	gint res = gtk_dialog_run (GTK_DIALOG(dialog));
	if(res == GTK_RESPONSE_ACCEPT){  //open file
		GtkFileChooser * chooser = GTK_FILE_CHOOSER(dialog);
		char *filename = gtk_file_chooser_get_filename(chooser);
		if(filename == NULL) {
			gtk_label_set_text(textResultLabel, "Can not open the file.");
			return;
		}
		FILE *file = fopen(filename,"r");
		if (file == NULL) {
			gtk_label_set_text(textResultLabel, "Can not open the file.");
			g_free(filename);
			return;
		}
    	char num;
		int cell = 0;
		on_clear_clicked();
    	while((num=getc(file)) != EOF){ //get charcater from file
			//exception handler
			if ((num < 48 || num > 57) && num != ' ' && num != '\n') {
				gtk_label_set_text(textResultLabel, "The file contains an illegal character or is an incompatible file.");
				incompatibleFile = 1;
				break;
			}
			//loadButton the sudoku clues
			else if (num != ' ' && num != '\n'){
				if (cell == 81){ //exception handler
					gtk_label_set_text(textResultLabel, "The file contains more numbers than the expected.");
					incompatibleFile = 1;
					break;
				}
				buttons[cell].num = num-48;
				//print in button
				char strNum[5];
				snprintf(strNum, sizeof(strNum),"%d",buttons[cell].num);
				if(buttons[cell].num  != 0) {
					GdkColor color;
        			gdk_color_parse ("#FF0000", &color);
        			gtk_widget_modify_fg (buttons[cell].button, GTK_STATE_NORMAL, &color);
					gtk_button_set_label(buttons[cell].button, strNum);
					buttons[cell].pista = TRUE; //clue button
				}else {
					gtk_button_set_label(buttons[cell].button, " ");
					buttons[cell].pista = FALSE; //no clue
				}
				cell++;
			}
		}

    	fclose(file);
		if (incompatibleFile == 1){ //instructions for active exception handler
			clear = false;
			on_clear_clicked();
			clear = true;
			incompatibleFile = 0;
		}else {
			currentFileName = filename;
			gtk_label_set_text(fileName, g_path_get_basename(currentFileName));
		}
		g_free(filename);
	}  
	gtk_widget_destroy(dialog);

}

int main(int argc, char *argv[]) {

	gtk_init(&argc, &argv); // init Gtk

	builder = gtk_builder_new_from_file ("sudoku.glade");
 
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	//creates grids 
	for(int i = 0;i<11;i++) {
		char str[5]; // Changed to a single character array
		snprintf(str, sizeof(str), "%d", i);
		char strConc[10] = "g"; // Changed to a single character array
		strcat(strConc, str);
		grids[i] = GTK_WIDGET(gtk_builder_get_object(builder, strConc));
	}

	//creates and adds buttons to array
	for(int i = 0;i<81;i++) {
		int j = i + 1;
		char str[5]; // Changed to a single character array
		snprintf(str, sizeof(str), "%d", j);
		char strConc[10] = "b"; // Changed to a single character array
		strcat(strConc, str);
		GtkWidget *button = GTK_WIDGET(gtk_builder_get_object(builder, strConc));
		buttons[i].button = GTK_BUTTON(button);
        buttons[i].num = 0;
		buttons[i].pista = false;
		//connect signals
		ButtonData *b = &buttons[i];
		g_signal_connect(G_OBJECT(buttons[i].button), "clicked", G_CALLBACK(on_b_clicked), (gpointer) b);
	}

	//create used buttons
	fixedElement = GTK_WIDGET(gtk_builder_get_object(builder, "f1"));
	solveButton = GTK_WIDGET(gtk_builder_get_object(builder, "solveButton"));
	g_signal_connect(G_OBJECT(solveButton), "clicked", G_CALLBACK(on_resolver_clicked),NULL);
	clearButton = GTK_WIDGET(gtk_builder_get_object(builder, "clearButton"));
	g_signal_connect(G_OBJECT(clearButton), "clicked", G_CALLBACK(on_clear_clicked),NULL);
	exitButton = GTK_WIDGET(gtk_builder_get_object(builder, "exitButton"));
	g_signal_connect(G_OBJECT(exitButton), "clicked", G_CALLBACK(on_exit_clicked), NULL);
	
	//create menu item buttons
	newButton =GTK_WIDGET(gtk_builder_get_object(builder, "newButton"));
	g_signal_connect(newButton, "activate", G_CALLBACK(on_new_clicked), NULL);
	loadButton = GTK_WIDGET(gtk_builder_get_object(builder, "loadButton"));
	g_signal_connect(loadButton, "activate", G_CALLBACK(on_loadButton_clicked), NULL);
	saveAsButton = GTK_WIDGET(gtk_builder_get_object(builder, "saveAsButton"));
	g_signal_connect(saveAsButton, "activate", G_CALLBACK(on_saveAsButton_clicked), NULL);
	saveButton = GTK_WIDGET(gtk_builder_get_object(builder, "saveButton"));
	g_signal_connect(saveButton,"activate",G_CALLBACK(on_saveButton_clicked), NULL);
	disabledMenu = GTK_WIDGET(gtk_builder_get_object(builder, "disabledMenu"));
	gtk_widget_set_visible(disabledMenu,FALSE); //set no visible for default
	timeC = GTK_WIDGET(gtk_builder_get_object(builder, "time"));

	//create labels
	textResultLabel = GTK_LABEL(gtk_builder_get_object(builder, "textResultLabel"));
	fileName = GTK_LABEL(gtk_builder_get_object(builder, "fileName"));

	stopwatch = g_timer_new();
	g_timer_stop (stopwatch);

	gtk_widget_show(window);
	gtk_main();
	return EXIT_SUCCESS;
}



