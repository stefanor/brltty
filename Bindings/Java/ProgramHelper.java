/*
 * libbrlapi - A library providing access to braille terminals for applications.
 *
 * Copyright (C) 2006-2020 by
 *   Samuel Thibault <Samuel.Thibault@ens-lyon.org>
 *   Sébastien Hinderer <Sebastien.Hinderer@ens-lyon.org>
 *
 * libbrlapi comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your option) any
 * later version. Please see the file LICENSE-LGPL for details.
 *
 * Web Page: http://brltty.app/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

package org.a11y.brlapi;

public abstract class ProgramHelper {
  protected ProgramHelper () {
  }

  public static void writeMessage (String format, Object... arguments) {
    System.err.println(String.format(format, arguments));
  }

  public static void syntaxError (String format, Object... arguments) {
    writeMessage(format, arguments);
    System.exit(2);
  }

  public static void semanticError (String format, Object... arguments) {
    writeMessage(format, arguments);
    System.exit(3);
  }

  public static void internalError (String format, Object... arguments) {
    writeMessage(format, arguments);
    System.exit(4);
  }

  protected final void tooManyParameters () throws OperandException {
    throw new OperandException("too many parameters");
  }

  protected static Parameter getParameter (Connection connection, String name) {
    Parameter parameter = connection.getParameters().get(name);

    if (parameter == null) {
      semanticError("unknown parameter: %s", name);
    }

    return parameter;
  }

  public static void show (String line) {
    System.out.println(line);
  }

  public static void show (String label, String value) {
    show((label + ": " + value));
  }
}
