<div class="related-pages">
  {# mod: Per-page navigation #}
  {% if meta %}
    {% if 'sequential_nav' in meta %}
      {% set sequential_nav = meta.sequential_nav %}
    {% endif %}
  {% endif %}
  {# mod: Conditional wrappers to control page navigation buttons #}
  {% if sequential_nav != "none" -%}
    {% if next and (sequential_nav == "next" or sequential_nav == "both") -%}
      <a class="next-page" href="{{ next.link }}">
        <div class="page-info">
          <div class="context">
            <span>{{ _("Next") }}</span>
          </div>
          <div class="title">{{ next.title }}</div>
        </div>
        <svg class="furo-related-icon"><use href="#svg-arrow-right"></use></svg>
      </a>
    {%- endif %}
    {% if prev and (sequential_nav == "prev" or sequential_nav == "both") -%}
      <a class="prev-page" href="{{ prev.link }}">
        <svg class="furo-related-icon"><use href="#svg-arrow-right"></use></svg>
        <div class="page-info">
          <div class="context">
            <span>{{ _("Previous") }}</span>
          </div>
          {% if prev.link == pathto(root_doc) %}
            <div class="title">{{ _("Home") }}</div>
          {% else %}
            <div class="title">{{ prev.title }}</div>
          {% endif %}
        </div>
      </a>
    {%- endif %}
  {%- endif %}
</div>
<div class="bottom-of-page">
  <div class="left-details">
    {%- if show_copyright %}
    <div class="copyright">
      {%- if hasdoc('copyright') %}
        {% trans path=pathto('copyright'), copyright=copyright|e -%}
          <a href="{{ path }}">Copyright</a> &#169; {{ copyright }}
        {%- endtrans %}
      {%- else %}
        {% trans copyright=copyright|e -%}
          Copyright &#169; {{ copyright }}
        {%- endtrans %}
      {%- endif %}
    </div>
    {%- endif %}

    {# mod: removed "Made with" #}

    {%- if last_updated -%}
    <div class="last-updated">
      {% trans last_updated=last_updated|e -%}
        Last updated on {{ last_updated }}
      {%- endtrans -%}
    </div>
    {%- endif %}

    {%- if show_source and has_source and sourcename %}
    <div class="show-source">
      <a class="muted-link" href="{{ pathto('_sources/' + sourcename, true)|e }}"
         rel="nofollow">Show source</a>
    </div>
    {%- endif %}
  </div>
  <div>
   {% if has_contributor_listing and display_contributors and pagename and page_source_suffix %}
       {% set contributors = get_contributors_for_file(pagename, page_source_suffix) %}
       {% if contributors %}
          {% if contributors | length > 1 %}
              <a class="display-contributors">Thanks to the {{ contributors |length }} contributors!</a>
          {% else %}
              <a class="display-contributors">Thanks to our contributor!</a>
          {% endif %}
          <div id="overlay"></div>
          <ul class="all-contributors">
              {% for contributor in contributors %}
                  <li>
                      <a href="{{ contributor[1] }}" class="contributor">{{ contributor[0] }}</a>
                  </li>
              {% endfor %}
          </ul>
       {% endif %}
   {% endif %}
  </div>
  <div class="right-details"><a href="" class="js-revoke-cookie-manager muted-link">Manage your tracker settings</a></div>
</div>