<?php

/**
 * @file
 * Generates the EPUB versions of the pages.
 *
 * This file is included by the print_epub module and includes the
 * functions that interface with the EPUB generation packages.
 *
 * @ingroup print
 */

module_load_include('inc', 'print', 'print.pages');

/**
 * Generate a EPUB version of the printer-friendly page.
 *
 * @see print_controller()
 * @see _print_epub_domepub()
 * @see _print_epub_tcepub()
 */
function print_epub_controller() {
  // Disable caching for generated EPUBs, as Backdrop doesn't ouput the proper
  // headers from the cache.
  $GLOBALS['conf']['cache'] = FALSE;

  $args = func_get_args();
  $path = filter_xss(implode('/', $args));
  $cid = isset($_GET['comment']) ? (int) $_GET['comment'] : NULL;

  // Handle the query.
  $query = $_GET;
  unset($query['q']);

  $node = NULL;
  if (!empty($path)) {
    if ($alias = backdrop_lookup_path('source', $path)) {
      // Alias.
      $path_arr = explode('/', $alias);
      $node = node_load($path_arr[1]);
    }
    elseif (ctype_digit($args[0])) {
      // Normal nid.
      $node = node_load($args[0]);
    }

    $epub_filename = config_get('print_epub.settings', 'print_epub_filename');
    if (!empty($epub_filename) && !empty($node)) {
      $epub_filename = token_replace($epub_filename, array('node' => $node), array('clear' => TRUE));
    }
    else {
      $epub_filename = token_replace($epub_filename, array('site'), array('clear' => TRUE));
      if (empty($epub_filename)) {
        // If empty, use a fallback solution.
        $epub_filename = str_replace('/', '_', $path);
      }
    }
  }
  else {
    $epub_filename = 'page';
  }

  if (function_exists('transliteration_clean_filename')) {
    $epub_filename = transliteration_clean_filename($epub_filename, language_default('language'));
  }

  backdrop_alter('print_epub_filename', $epub_filename, $path);

  $epub = print_epub_generate_path($path, $query, $cid, $epub_filename . '.epub');
  if ($epub == NULL) {
    backdrop_goto($path);
    exit;
  }

  $nodepath = (isset($node->nid)) ? 'node/' . $node->nid : backdrop_get_normal_path($path);
  db_merge('print_epub_page_counter')
    ->key(array('path' => substr($nodepath, 0, 255)))
    ->fields(array(
    'totalcount' => 1,
    'timestamp' => REQUEST_TIME,
  ))
    ->expression('totalcount', 'totalcount + 1')
    ->execute();

  backdrop_exit();
}

/**
 * Gennerate a EPUB for a given Backdrop path.
 *
 * @param string $path
 *   path of the page to convert to EPUB.
 * @param array $query
 *   (Optional) array of key/value pairs as used in the url() function for the
 *   query.
 * @param int $cid
 *   (Optional) comment ID of the comment to render.
 * @param string $epub_filename
 *   (Optional) filename of the generated EPUB.
 * @param string $view_mode
 *   (Optional) view mode to be used when rendering the content.
 *
 * @return string|null
 *   generated EPUB page, or NULL in case of error
 *
 * @see print_epub_controller()
 */
function print_epub_generate_path($path, $query = NULL, $cid = NULL, $epub_filename = NULL, $view_mode = PRINT_VIEW_MODE) {
  $link = print_epub_print_link();
  $node = print_controller($path, $link['format'], $cid, $view_mode);
  if ($node) {
    $html = theme('print', array(
      'node' => $node,
      'query' => $query,
      'expand_css' => TRUE,
      'format' => $link['format'],
    ));

    $meta = array(
      'node' => $node,
      'url' => url(backdrop_get_path_alias(empty($node->nid) ? $node->path : "node/$node->nid"), array('absolute' => TRUE)),
    );
    if (isset($node->name)) {
      $meta['name'] = $node->name;
    }
    if (isset($node->title)) {
      $meta['title'] = $node->title;
    }

    return print_epub_generate_html($html, $meta, $epub_filename);
  }
  else {
    return NULL;
  }
}
