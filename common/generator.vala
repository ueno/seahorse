/*
 * Seahorse
 *
 * Copyright (C) 2015 Daiki Ueno
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse {

public interface Generator : GLib.Object {
	public abstract string name { get; }
	public abstract GLib.ActionGroup actions { owned get; }
	public abstract GLib.MenuModel menu { owned get; }

	public void register() {
		Registry.register_object(this, "generator");
	}

	public static GLib.List<Generator> get_registered() {
		return (GLib.List<Seahorse.Generator>)Registry.object_instances("generator");
	}
}

}
