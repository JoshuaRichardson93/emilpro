// FIXME! Refactor this file...

#include <gtkmm.h>
#include <gtksourceviewmm.h>

#include <model.hh>
#include <idisassembly.hh>
#include <architecturefactory.hh>
#include <symbolfactory.hh>
#include <utils.hh>
#include <jumptargetdisplay.hh>
#include <hexview.hh>
#include <infobox.hh>
#include <symbolview.hh>
#include <instructionview.hh>
#include <sourceview.hh>
#include <emilpro.hh>
#include <server.hh>
#include <namemanglerview.hh>

#include <string>
#include <vector>

#include <emilpro_glade.hh>

using namespace emilpro;

class EmilProGui
{
public:
	EmilProGui() :
		m_nLanes(4)
	{
	}

	~EmilProGui()
	{
	}

	void init(int argc, char **argv)
	{
		m_app = new Gtk::Main(argc, argv);
		Gsv::init();

		m_hexView.init();

		m_builder = Gtk::Builder::create_from_string(glade_file);

		NameManglerView::instance().init(m_builder);

		Gtk::ImageMenuItem *fileOpenItem;
		Gtk::ImageMenuItem *viewForwardItem;
		Gtk::ImageMenuItem *viewBackwardItem;
		m_builder->get_widget("file_menu_open", fileOpenItem);
		m_builder->get_widget("view_menu_forward", viewForwardItem);
		m_builder->get_widget("view_menu_backward", viewBackwardItem);
		panic_if (!(fileOpenItem && viewForwardItem && viewBackwardItem),
				"Can't get menu items");
		fileOpenItem->signal_activate().connect(sigc::mem_fun(*this, &EmilProGui::onFileOpen));

		viewForwardItem->signal_activate().connect(sigc::mem_fun(*this, &EmilProGui::onViewForward));
		viewBackwardItem->signal_activate().connect(sigc::mem_fun(*this, &EmilProGui::onViewBackward));

		// FIXME! Get this from properties instead!
		m_backgroundColor = Gdk::Color("white");


		Gtk::ScrolledWindow *hexView8Bit, *hexView16Bit, *hexView32Bit, *hexView64Bit;
		m_builder->get_widget("hex_data_8bit_scrolledwindow", hexView8Bit);
		m_builder->get_widget("hex_data_16bit_scrolledwindow", hexView16Bit);
		m_builder->get_widget("hex_data_32bit_scrolledwindow", hexView32Bit);
		m_builder->get_widget("hex_data_64bit_scrolledwindow", hexView64Bit);

		panic_if(!hexView64Bit, "Can't get hexview");

		Gtk::TextView &tv8 = m_hexView.getTextView(8);
		Gtk::TextView &tv16 = m_hexView.getTextView(16);
		Gtk::TextView &tv32 = m_hexView.getTextView(32);
		Gtk::TextView &tv64 = m_hexView.getTextView(64);

		hexView8Bit->add(tv8);
		hexView16Bit->add(tv16);
		hexView32Bit->add(tv32);
		hexView64Bit->add(tv64);

		Gtk::FontButton *sourceFont;
		m_builder->get_widget("source_font", sourceFont);
		panic_if(!sourceFont,
				"Can't get source font");

		tv8.override_font(Pango::FontDescription(sourceFont->get_font_name()));
		tv16.override_font(Pango::FontDescription(sourceFont->get_font_name()));
		tv32.override_font(Pango::FontDescription(sourceFont->get_font_name()));
		tv64.override_font(Pango::FontDescription(sourceFont->get_font_name()));

		Gtk::ColorButton *historyColor;
		m_builder->get_widget("history_color2", historyColor);
		m_hexView.setMarkColor(historyColor->get_color());

		tv8.show();
		tv16.show();
		tv32.show();
		tv64.show();

		Gtk::Entry *lookupEntry;
		m_builder->get_widget("symbol_lookup_entry", lookupEntry);
		panic_if(!lookupEntry, "Can't get entry");

		lookupEntry->signal_activate().connect(sigc::mem_fun(*this,
				&EmilProGui::onEntryActivated));

		m_builder->get_widget("instructions_data_notebook", m_instructionsDataNotebook);
		panic_if(!m_instructionsDataNotebook, "Can't get notebook");

		Gtk::MenuItem *viewToggleInstructionsDataItem;
		m_builder->get_widget("view_toggle_instructions_data", viewToggleInstructionsDataItem);
		viewToggleInstructionsDataItem->signal_activate().connect(sigc::mem_fun(*this, &EmilProGui::onToggleInstructionsData));


		m_infoBox.init(m_builder);
		m_sourceView.init(m_builder);
		m_instructionView.init(m_builder, &m_hexView, &m_infoBox, &m_sourceView, &m_symbolView, &m_addressHistory);
		m_symbolView.init(m_builder, &m_instructionView, &m_hexView);
	}

	void run(int argc, char *argv[])
	{
		if (argc > 1) {
			const char *file = argv[1];
			void *data;
			size_t sz;

			data = read_file(&sz, "%s", file);
			if (data) {
				Model::instance().addData(data, sz);
			}

			refresh();
		}

		Gtk::Window * mainWindow = NULL;
		m_builder->get_widget("main_window", mainWindow);

		m_app->run(*mainWindow);
	}

protected:

	void refresh()
	{
		Model::instance().parseAll();

		m_hexView.clearData();
		m_symbolView.refreshSymbols();

		m_hexView.update();
	}

	void onFileOpen()
	{
		Gtk::FileChooserDialog *openFile = NULL;
		m_builder->get_widget("file_chooser", openFile);

		panic_if (!openFile,
				"Open file dialogue does not exist");

		int v = openFile->run();

		openFile->hide();

		if (v != Gtk::RESPONSE_ACCEPT)
			return;

		size_t sz;

		void *data = read_file(&sz, "%s", openFile->get_filename().c_str());

		if (!data)
			return; // FIXME! Do something

		Model::instance().destroy();
		SymbolFactory::instance().destroy();
		IDisassembly::instance().destroy();
		ArchitectureFactory::instance().destroy();

		if (!Model::instance().addData(data, sz))
			return;

		refresh();
	}

private:
	void onViewBackward()
	{
		updateHistoryEntry(m_addressHistory.back());
	}

	void onViewForward()
	{
		updateHistoryEntry(m_addressHistory.forward());
	}

	void updateHistoryEntry(const AddressHistory::Entry &e)
	{
		if (!e.isValid())
			return;

		m_instructionView.disableHistory();
		m_symbolView.update(e.getAddress());
		m_instructionView.enableHistory();

	}

	void onEntryActivated()
	{
		m_addressHistory.clear();
	}

	void onToggleInstructionsData()
	{
		m_instructionsDataNotebook->set_current_page(!m_instructionsDataNotebook->get_current_page());

	}


	typedef Gtk::TreeModel::Children TreeModelChildren_t;
	typedef std::list<Gtk::TreeModel::iterator> InstructionIterList_t;

	Gtk::Main *m_app;
	Glib::RefPtr<Gtk::Builder> m_builder;

	unsigned m_nLanes;

	Gdk::Color m_historyColors[3];
	Gdk::Color m_backgroundColor;

	HexView m_hexView;
	InfoBox m_infoBox;
	InstructionView m_instructionView;
	SymbolView m_symbolView;
	SourceView m_sourceView;
	AddressHistory m_addressHistory;
	Gtk::Notebook *m_instructionsDataNotebook;
};

int main(int argc, char **argv)
{
	EmilPro::init();

	EmilProGui *gui = new EmilProGui();

	gui->init(argc, argv);

	Server::instance().connect();

	gui->run(argc, argv);

	delete gui;

	EmilPro::destroy();

	return 0;
}
