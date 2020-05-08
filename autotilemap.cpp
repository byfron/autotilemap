#include "autotilemap.h"
#include "core/io/json.h"
#include "core/ustring.h"
#include "scene/resources/text_file.h"
#include "core/os/os.h"
#include "scene/2d/tile_map.h"

int encode_tile_and_flipping(int tid, int fx, int fy, int atlas_id=0) {
	
	//24 bits of id code. We encode the ATLAS id in 8 bits, and the tile_id in 16 bits
	int atlas_id_code = (int(atlas_id) & 0xff) << 16;
	int id_code = int(tid) & 0x0000ffff;
	
	//1 bit of flipx
	//1 bit of flipy
	int flip_x_code = int(fx) << 29;
	int flip_y_code = int(fy) << 30;
	
	//1 bit of transpose		
	int id = atlas_id_code + id_code + flip_x_code + flip_y_code;
	return id;
}

bool compute_flip_h(int code) {
	return bool((code >> 30) & 0x1);
}

bool compute_flip_v(int code) {
	return bool((code >> 29) & 0x1);
}

int compute_tile_id(int code) {
	return int(code & 0x0000FFFF);
}

int compute_atlas_id(int code) {
	return int((code >> 16) & 0x000000FF);
}

Vector2 compute_subtile_coords(int code) {
	int tid = compute_tile_id(code);
	int y = int(tid/7);
	int x = int(tid)%7;
	return Vector2(x, y);
}

Autotilemap::Autotilemap() {
}

void Autotilemap::init(const Vector2& top_left, const Vector2& bottom_right, const String& json_file) {
	_top_left = top_left;
	_bottom_right = bottom_right;
	_width = bottom_right.x - top_left.x;
	_height = bottom_right.y - top_left.y;
	load_from_json(json_file);

	_data.resize(_width * _height);
	
	_base_tile = _json_data.get("base_tile");
	if (_base_tile >= 0) {
		for (int r = 0; r < _height; r++) {
			for (int c = 0; c < _width; c++) {
				_data.set(r + c * _height, _base_tile);
			}
		}		
	}

	Dictionary data_dict = _json_data;
	
	if (data_dict.has("id_to_atlas")) {
		_id_to_atlas = _json_data.get("id_to_atlas");
	}

	if (data_dict.has("blob_autotiling")) {
		_blob_mode = true;
		Vector<Variant> codes = _json_data.get("codes");
		Vector<Variant> blob_autot = _json_data.get("blob_autotiling");
		for (int i = 0; i < blob_autot.size(); i++) {
			BlobAutotiler* autot = memnew(BlobAutotiler);			
			autot->init(blob_autot[i], codes);
			_autotilers.push_back(Ref<BlobAutotiler>(autot));
		}
	}

	if (data_dict.has("blob_terrain_autotiling")) {
		_blob_mode = true;
		Vector<Variant> codes = _json_data.get("codes");
		Vector<Variant> blob_autot = _json_data.get("blob_terrain_autotiling");
		for (int i = 0; i < blob_autot.size(); i++) {
			BlobTerrainAutotiler* autot = memnew(BlobTerrainAutotiler);			
			autot->init(blob_autot[i], codes);
			_autotilers.push_back(Ref<BlobTerrainAutotiler>(autot));
		}
	}

	if (data_dict.has("autotiling")) {
		Vector<Variant> quad_autot = _json_data.get("autotiling");
		for (int i = 0; i < quad_autot.size(); i++) {
			QuadAutotiler* autot = memnew(QuadAutotiler);
			autot->init(quad_autot[i]);
			_autotilers.push_back(Ref<QuadAutotiler>(autot));
		}
	}
}

void Autotilemap::apply(Object* obj_tilemap) {
	if (obj_tilemap) {
		TileMap* tilemap = cast_to<TileMap>(obj_tilemap);
		map_ids_to_tiles(tilemap);
		apply_autotiling(tilemap);
		apply_blob_autotiling(tilemap);
		apply_blob_terrain_autotiling(tilemap);
	}
	else {
		print_error("Error applying tilemap");
	}
}


void Autotilemap::map_ids_to_tiles(TileMap* tilemap) {
	if (_id_to_atlas.size() > 0) {
		for (int i = 0; i < _id_to_atlas.size(); i++) {
			Variant id_to_atlas = _id_to_atlas[i];
			for (int y = 1; y < _height-1; y++) {
				for (int x = 1; x < _width-1; x++) {

					//TODO access data only once for all autotilers!					
					int src_tile_id = _data[y + x*_height];
					if (src_tile_id == int(id_to_atlas.get("src_tile"))) {
						int atlas_id = id_to_atlas.get("atlas");
						tilemap->set_cell(_top_left.x + x, _top_left.y + y, atlas_id,
										  false, false, false, Vector2(0,0));
					}					
				}
			}
		}
	}
}

void Autotilemap::apply_autotiling(TileMap* tilemap) {
	for (int ai = 0; ai < _autotilers.size(); ai++) {
		if (not _autotilers[ai]->is_type("QuadAutotiler")) {
			continue;			
		}

		Ref<QuadAutotiler> autotiler = Ref<QuadAutotiler>(_autotilers[ai]);
		
		for (int y = 1; y < _height-1; y++) {
			for (int x = 1; x < _width-1; x++) {
		
				int src_tile_id = _data[y + x*_height];
				if (not autotiler->is_source_tile(src_tile_id)) {
					//tilemap->set_cell(_top_left.x + x, _top_left.y + y, 0,
					//				  false, false, false, Vector2(0,0));
					continue;
				}

				int n = int(autotiler->is_neighbor_tile(_data[(y-1) + x*_height]));
				int e = int(autotiler->is_neighbor_tile(_data[y + (x+1)*_height]));
				int s = int(autotiler->is_neighbor_tile(_data[(y+1) + x*_height]));
				int w = int(autotiler->is_neighbor_tile(_data[y + (x-1)*_height]));
				
				int v = n + 4*e + 16*s + 64*w;
				
				if (autotiler->get_metadata_map().has(v)) {						
					auto code = autotiler->get_metadata_map()[v];
					
					tilemap->set_cell(_top_left.x + x, _top_left.y + y, code.get("id"),
									  code.get("x_mirror"), code.get("y_mirror"), false, Vector2(0,0));
				}
				// else {
				// 	tilemap->set_cell(_top_left.x + x, _top_left.y + y, _base_tile,
				// 					  false, false, false, Vector2(0,0));

				// }
			}
		}
	}
}

void Autotilemap::apply_blob_terrain_autotiling(TileMap* tilemap) {

	int n = 0;
	int ne = 0;
	int e = 0;
	int se = 0;
	int s = 0;
	int sw = 0;
	int w = 0;
	int nw = 0;
	
	for (int ai = 0; ai < _autotilers.size(); ai++) {
		if (not _autotilers[ai]->is_type("BlobTerrainAutotiler")) {
			continue;			
		}
		
		Ref<BlobTerrainAutotiler> autotiler = Ref<BlobTerrainAutotiler>(_autotilers[ai]);
		
		for (int y = 1; y < _height-1; y++) {
			for (int x = 1; x < _width-1; x++) {
				int src_tile_id = _data[y + x*_height];

				//TODO: use a map of autotilers?
				// entry ID has the list of autotilers
				if (not autotiler->is_source_tile(src_tile_id)) {
					continue;
				}

				int nt = _data[(y-1) + x*_height];
				int net = _data[(y-1) + (x+1)*_height];
				int et = _data[y + (x+1)*_height];
				int set = _data[(y+1) + (x+1)*_height];
				int st = _data[(y+1) + x*_height];
				int swt = _data[(y+1) + (x-1)*_height];
				int wt = _data[y + (x-1)*_height];
				int nwt = _data[(y-1) + (x-1)*_height];

				Vector<int> all_neighbors;
				all_neighbors.push_back(nt);
				all_neighbors.push_back(net);
				all_neighbors.push_back(et);
				all_neighbors.push_back(set);
				all_neighbors.push_back(st);
				all_neighbors.push_back(swt);
				all_neighbors.push_back(wt);
				all_neighbors.push_back(nwt);

				bool flag1 = false;
				bool flag2 = false;
				for (int i = 0; i < all_neighbors.size(); i++) {
					int n = all_neighbors[i];
					if (n == autotiler->get_src_1()) {
						flag1 = true;
					}
					if (n == autotiler->get_src_2()) {
						flag2 = true;
					}
				}

				if (not (flag1 and flag2)) {
					bool isolated = true;
					for (int i = 0; i < all_neighbors.size(); i++) {
						int n = all_neighbors[i];
						if (n != autotiler->get_src_2()) {
							isolated = false;
							break;
						}
					}

					if (isolated) {
						int tile_id = autotiler->get_metadata_map()[0];
						Vector2 subtile_coords = compute_subtile_coords(tile_id);
						tilemap->set_cell(_top_left.x + x, _top_left.y + y, autotiler->get_atlas_id(),
										  false, false, false, subtile_coords);	  
					}
					continue;					
				}

				//TODO: We should re-factor/clean this				
				n = int(autotiler->is_neighbor_tile(nt));
				ne = int(autotiler->is_neighbor_tile(net));
				e = int(autotiler->is_neighbor_tile(et));
				se = int(autotiler->is_neighbor_tile(set));
				s = int(autotiler->is_neighbor_tile(st));
				sw = int(autotiler->is_neighbor_tile(swt));
				w = int(autotiler->is_neighbor_tile(wt));
				nw = int(autotiler->is_neighbor_tile(nwt));
				
				int v = n + 2*ne + 4*e + 8*se + 16*s + 32*sw + 64*w + 128*nw;

				bool flip_h = false;
				bool flip_v = false;
				bool transpose = false;
				int tile_id = 0;
				Vector2 subtile_coords = Vector2(0,0);
				
				if (v > 0) {
					if (autotiler->get_metadata_map().has(v)) {						
						tile_id = autotiler->get_metadata_map()[v];
						if (_blob_mode) {
							subtile_coords = compute_subtile_coords(tile_id);
							tile_id = autotiler->get_atlas_id();
						}
						tilemap->set_cell(_top_left.x + x, _top_left.y + y, tile_id,
										  flip_h, flip_v, transpose, subtile_coords);
					}
					else {
						tilemap->set_cell(_top_left.x + x, _top_left.y + y, _base_tile,
										  flip_h, flip_v, transpose, subtile_coords);
					}
				}				
			}
		}
	}
}

void Autotilemap::apply_blob_autotiling(TileMap* tilemap) {

	int n = 0;
	int ne = 0;
	int e = 0;
	int se = 0;
	int s = 0;
	int sw = 0;
	int w = 0;
	int nw = 0;
	
	for (int ai = 0; ai < _autotilers.size(); ai++) {
		if (not _autotilers[ai]->is_type("BlobAutotiler")) {
			continue;					
		}

		Ref<Autotiler> autotiler = _autotilers[ai];
		
		for (int y = 1; y < _height-1; y++) {
			for (int x = 1; x < _width-1; x++) {
				int src_tile_id = _data[y + x*_height];
				if (not autotiler->is_source_tile(src_tile_id)) {
					tilemap->set_cell(_top_left.x + x, _top_left.y + y, 0,
									  false, false, false, Vector2(0,0));
					continue;
				}
				n = int(autotiler->is_neighbor_tile(_data[(y-1) + x*_height]));
				ne = int(autotiler->is_neighbor_tile(_data[(y-1) + (x+1)*_height]));
				e = int(autotiler->is_neighbor_tile(_data[y + (x+1)*_height]));
				se = int(autotiler->is_neighbor_tile(_data[(y+1) + (x+1)*_height]));
				s = int(autotiler->is_neighbor_tile(_data[(y+1) + x*_height]));
				sw = int(autotiler->is_neighbor_tile(_data[(y+1) + (x-1)*_height]));
				w = int(autotiler->is_neighbor_tile(_data[y + (x-1)*_height]));
				nw = int(autotiler->is_neighbor_tile(_data[(y-1) + (x-1)*_height]));
		
				int v = n + 2*ne + 4*e + 8*se + 16*s + 32*sw + 64*w + 128*nw;

				bool flip_h = false;
				bool flip_v = false;
				bool transpose = false;
				int tile_id = 0;
				Vector2 subtile_coords = Vector2(0,0);
				
				if (v > 0) {
					if (autotiler->get_metadata_map().has(v)) {						
						tile_id = autotiler->get_metadata_map()[v];
						if (_blob_mode) {
							subtile_coords = compute_subtile_coords(tile_id);
							tile_id = autotiler->get_atlas_id();
						}											
					}
				}
				tilemap->set_cell(_top_left.x + x, _top_left.y + y, tile_id,
								  flip_h, flip_v, transpose, subtile_coords);	  
			}
		}
	}
}

void Autotilemap::set_submap(Vector2 sub_top_left, int width, int height, const Variant& input_data) {
	const Vector<int32_t>& idata = input_data;
	Vector2 offset = sub_top_left - _top_left;
	for (int row = 0; row < height; row++) {		
		for (int col = 0; col < width; col++) {
			_data.set(row + offset.y + (col + offset.x) * _height, idata[row + col * height]);
		}
	}
}

void Autotilemap::set_value(int x, int y, int v) {
	_data.set(y + x * _height, v);
}

void Autotilemap::set_value_relative_to_tl(int x, int y, int v) {
	set_value(x - _top_left.x, y - _top_left.y, v);
}

void Autotilemap::load_from_json(const String& json_file) {

	Error err;
	FileAccessRef f = FileAccess::open(json_file, FileAccess::READ, &err);
	if (!f) {
		print_line("Error loading");
	}

	Vector<uint8_t> array;
	array.resize(f->get_len());
	f->get_buffer(array.ptrw(), array.size());
	String text;
	text.parse_utf8((const char *)array.ptr(), array.size());
	Variant ret;
	Dictionary data;
	String err_message;
	int err_line;
	Error file_err;

	Variant json_data_tmp;
	if (OK != JSON::parse(text, json_data_tmp, err_message, err_line)) {
		print_line("Error loading json file");
	}

	_json_data = json_data_tmp;
}

Vector2 Autotilemap::get_top_left() {
	return _top_left;
}

Vector2 Autotilemap::get_bottom_right() {
	return _bottom_right;
}

int Autotilemap::get_width() {
	return _width;
}

int Autotilemap::get_height() {
	return _height;
}

Variant Autotilemap::get_data() {
	//TODO can we avoid this copy?
	return _data;
}

void Autotilemap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("init", "top_left", "bottom_right", "json_file"), &Autotilemap::init);
	ClassDB::bind_method(D_METHOD("set_value", "x", "y", "v"), &Autotilemap::set_value);
	ClassDB::bind_method(D_METHOD("set_value_relative_to_tl", "x", "y", "v"), &Autotilemap::set_value_relative_to_tl);
	ClassDB::bind_method(D_METHOD("set_submap", "sub_top_left", "width", "height", "idata"), &Autotilemap::set_submap);
	ClassDB::bind_method(D_METHOD("apply"), &Autotilemap::apply);
	ClassDB::bind_method(D_METHOD("get_top_left"), &Autotilemap::get_top_left);
	ClassDB::bind_method(D_METHOD("get_bottom_right"), &Autotilemap::get_bottom_right);
	ClassDB::bind_method(D_METHOD("get_width"), &Autotilemap::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &Autotilemap::get_height);
	ClassDB::bind_method(D_METHOD("get_data"), &Autotilemap::get_data);
}
